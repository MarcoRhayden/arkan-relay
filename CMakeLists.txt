cmake_minimum_required(VERSION 3.26)
project(arkan_relay LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Toolchain / flags
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# spdlog (CONFIG → fallback header-only)
# ---------------------------------------------------------------------------
find_package(spdlog CONFIG QUIET)
if(NOT spdlog_FOUND)
  message(STATUS "spdlog: falling back to header-only include (Homebrew/vcpkg)")
  find_path(SPDLOG_INCLUDE_DIR
    NAMES spdlog/spdlog.h
    PATHS $ENV{HOMEBREW_PREFIX}/include /opt/homebrew/include /usr/local/include
  )
  if(NOT SPDLOG_INCLUDE_DIR)
    message(FATAL_ERROR "spdlog header not found. Install via Homebrew/vcpkg or set SPDLOG_INCLUDE_DIR.")
  endif()
  add_library(spdlog::spdlog INTERFACE IMPORTED)
  set_target_properties(spdlog::spdlog PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${SPDLOG_INCLUDE_DIR}")
endif()

# ---------------------------------------------------------------------------
# toml++ (CONFIG → fallback header-only)
# ---------------------------------------------------------------------------
find_package(tomlplusplus CONFIG QUIET)
if(NOT tomlplusplus_FOUND)
  message(STATUS "tomlplusplus: falling back to header-only include (Homebrew/vcpkg)")
  find_path(TOMLPP_INCLUDE_DIR
    NAMES toml++/toml.hpp
    PATHS /opt/homebrew/opt/tomlplusplus/include /opt/homebrew/include /usr/local/include
  )
  if(NOT TOMLPP_INCLUDE_DIR)
    message(FATAL_ERROR "toml++ header not found. Install via Homebrew/vcpkg or set TOMLPP_INCLUDE_DIR.")
  endif()
  add_library(tomlplusplus::tomlplusplus INTERFACE IMPORTED)
  set_target_properties(tomlplusplus::tomlplusplus PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${TOMLPP_INCLUDE_DIR}")
endif()

# ---------------------------------------------------------------------------
# Boost (filesystem + headers; system if defined)
# ---------------------------------------------------------------------------
find_package(Boost 1.70 CONFIG QUIET COMPONENTS filesystem)
if(NOT Boost_FOUND OR NOT TARGET Boost::filesystem)
  message(STATUS "Boost config not fully found; falling back to FindBoost module")
  if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
  endif()
  set(Boost_NO_BOOST_CMAKE ON)
  find_package(Boost REQUIRED COMPONENTS filesystem)
endif()

# Filesystem lib target
if(TARGET Boost::filesystem)
  set(BOOST_FS_TARGET Boost::filesystem)
else()
  include_directories(${Boost_INCLUDE_DIRS})
  set(BOOST_FS_TARGET ${Boost_FILESYSTEM_LIBRARY})
endif()

# System lib target (optional; header-only OK ≥1.70)
if(TARGET Boost::system)
  set(BOOST_SYS_TARGET Boost::system)
else()
  if(DEFINED Boost_SYSTEM_LIBRARY AND Boost_SYSTEM_LIBRARY)
    set(BOOST_SYS_TARGET ${Boost_SYSTEM_LIBRARY})
  else()
    find_library(Boost_SYSTEM_LIBRARY
      NAMES boost_system
      PATHS $ENV{HOMEBREW_PREFIX}/lib /opt/homebrew/lib /usr/local/lib
    )
    if(Boost_SYSTEM_LIBRARY)
      set(BOOST_SYS_TARGET ${Boost_SYSTEM_LIBRARY})
    else()
      message(STATUS "Boost.System not found as lib; continuing without explicit link (header-only OK ≥1.70).")
      set(BOOST_SYS_TARGET "")
    endif()
  endif()
endif()

# Boost headers pseudo-target
set(BOOST_HDR_TARGET "")
if(TARGET Boost::headers)
  set(BOOST_HDR_TARGET Boost::headers)
elseif(TARGET Boost::boost)
  set(BOOST_HDR_TARGET Boost::boost)
endif()

# Threads
find_package(Threads QUIET)

# ---------------------------------------------------------------------------
# Layers / Targets
# ---------------------------------------------------------------------------
add_library(arkan_relay_domain INTERFACE)
target_include_directories(arkan_relay_domain INTERFACE src)

add_library(arkan_relay_application INTERFACE)
target_include_directories(arkan_relay_application INTERFACE src)
target_link_libraries(arkan_relay_application INTERFACE arkan_relay_domain)

set(INFRA_COMMON
  # domain
  src/domain/Settings.hpp

  # application (service)
  src/application/services/BridgeService.hpp
  src/application/services/BridgeService.cpp

  # application ports (headers only)
  src/application/ports/IConfigProvider.hpp
  src/application/ports/ILogger.hpp
  src/application/ports/IHook.hpp
  src/application/ports/IKoreLink.hpp
  src/application/ports/IFrameCodec.hpp

  # infrastructure - config/logging/link/codec
  src/infrastructure/config/Config_Toml.hpp
  src/infrastructure/config/Config_Toml.cpp
  src/infrastructure/logging/Logger_Spdlog.hpp
  src/infrastructure/logging/Logger_Spdlog.cpp
  src/infrastructure/link/KoreLink_Asio.hpp
  src/infrastructure/link/KoreLink_Asio.cpp
  src/infrastructure/codec/FrameCodec_Noop.hpp
)

add_library(arkan_relay_infrastructure STATIC ${INFRA_COMMON})
target_include_directories(arkan_relay_infrastructure PUBLIC src)
target_link_libraries(arkan_relay_infrastructure
  PUBLIC  arkan_relay_application
  PRIVATE ${BOOST_FS_TARGET} ${BOOST_SYS_TARGET} ${BOOST_HDR_TARGET}
          spdlog::spdlog tomlplusplus::tomlplusplus
          Threads::Threads
)

# Hook Win32
if(WIN32)
  target_sources(arkan_relay_infrastructure PRIVATE
    src/infrastructure/hook/Hook_Win32.hpp
    src/infrastructure/hook/Hook_Win32.cpp
  )
  target_link_libraries(arkan_relay_infrastructure PRIVATE ws2_32)
  target_compile_definitions(arkan_relay_infrastructure PRIVATE _WIN32_WINNT=0x0601)
endif()

# DLL (composition root)
if(WIN32)
  add_library(arkan_relay SHARED
    src/adapters/outbound/dll/DllMain.cpp
  )
  target_include_directories(arkan_relay PRIVATE src)
  target_link_libraries(arkan_relay PRIVATE arkan_relay_infrastructure)
  set_target_properties(arkan_relay PROPERTIES OUTPUT_NAME "ArkanRelay")
endif()

# ---------------------------------------------------------------------------
# Tests (GoogleTest)
# ---------------------------------------------------------------------------
include(CTest)
option(ARKAN_BUILD_TESTS "Build unit tests" ON)

if(ARKAN_BUILD_TESTS AND BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  include(GoogleTest)

  # tests que não dependem de WinAPI (OK em mac/Linux/Win)
  add_executable(arkan_relay_test_config  tests/test_config.cpp)
  target_link_libraries(arkan_relay_test_config PRIVATE
    arkan_relay_infrastructure GTest::gtest_main
    ${BOOST_FS_TARGET} ${BOOST_SYS_TARGET} ${BOOST_HDR_TARGET} Threads::Threads
  )
  gtest_discover_tests(arkan_relay_test_config)

  add_executable(arkan_relay_test_logger  tests/test_logger.cpp)
  target_link_libraries(arkan_relay_test_logger PRIVATE
    arkan_relay_infrastructure GTest::gtest_main
    ${BOOST_FS_TARGET} ${BOOST_SYS_TARGET} ${BOOST_HDR_TARGET} Threads::Threads
  )
  gtest_discover_tests(arkan_relay_test_logger)

  add_executable(arkan_relay_test_link    tests/test_link_asio.cpp)
  target_link_libraries(arkan_relay_test_link PRIVATE
    arkan_relay_infrastructure GTest::gtest_main
    ${BOOST_FS_TARGET} ${BOOST_SYS_TARGET} ${BOOST_HDR_TARGET} Threads::Threads
  )
  gtest_discover_tests(arkan_relay_test_link)
endif()
