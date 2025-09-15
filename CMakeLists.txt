cmake_minimum_required(VERSION 3.26)
project(arkan_relay LANGUAGES CXX)

# -----------------------------------------------------------------------------
# Build mode flags
# -----------------------------------------------------------------------------
option(ARKAN_RELEASE "Build only the DLL for release (no tests)" OFF)
option(ARKAN_BUILD_TESTS "Build unit tests" ON)

if(ARKAN_RELEASE)
  # In release-only builds we skip tests entirely.
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(ARKAN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
endif()

# -----------------------------------------------------------------------------
# Platform guard: this project is fixed to MSVC Win32 (x86)
# -----------------------------------------------------------------------------
if(NOT MSVC)
  message(FATAL_ERROR "This project is fixed to MSVC Win32 (x86).")
endif()

# Force 32-bit generator/platform
if(NOT DEFINED CMAKE_GENERATOR_PLATFORM OR CMAKE_GENERATOR_PLATFORM STREQUAL "")
  set(CMAKE_GENERATOR_PLATFORM "Win32" CACHE STRING "" FORCE)
elseif(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "Win32")
  message(FATAL_ERROR "CMAKE_GENERATOR_PLATFORM='${CMAKE_GENERATOR_PLATFORM}' not supported. Use Win32 (x86).")
endif()

# Double-check pointer size
if(DEFINED CMAKE_SIZEOF_VOID_P AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "64-bit architecture detected. Build must be x86 (32-bit).")
endif()

# vcpkg triplet (static CRT)
set(VCPKG_TARGET_TRIPLET "x86-windows-static" CACHE STRING "" FORCE)

# -----------------------------------------------------------------------------
# Toolchain / C++ flags
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
  add_definitions(-DUNICODE -D_UNICODE)
  # Static runtime (/MT and /MTd)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Reduce spdlog noise in Release builds
add_compile_definitions($<$<CONFIG:Release>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>)

# -----------------------------------------------------------------------------
# Dependencies (via vcpkg CONFIG)
# -----------------------------------------------------------------------------
find_package(spdlog CONFIG REQUIRED)          # provides spdlog::spdlog
find_package(tomlplusplus CONFIG REQUIRED)

# Boost (header-only/targets provided by vcpkg)
find_package(boost_filesystem CONFIG REQUIRED)  # Boost::filesystem
find_package(boost_system CONFIG REQUIRED)      # Boost::system
find_package(boost_asio CONFIG REQUIRED)        # Boost::asio

find_package(Threads REQUIRED)

# -----------------------------------------------------------------------------
# Layered targets
# -----------------------------------------------------------------------------
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

# Static library with the core infra
add_library(arkan_relay_infrastructure STATIC
  ${INFRA_COMMON}

  $<$<BOOL:${WIN32}>:
    src/infrastructure/hook/Hook_Win32.hpp
    src/infrastructure/hook/Hook_Win32.cpp

    src/infrastructure/hook/win32/AddressResolver.hpp
    src/infrastructure/hook/win32/AddressResolver.cpp
    src/infrastructure/hook/win32/MemoryAccess.hpp
    src/infrastructure/hook/win32/MemoryAccess.cpp
    src/infrastructure/hook/win32/SlotPatcher.hpp        
    src/infrastructure/hook/win32/SlotWatchdog.hpp
    src/infrastructure/hook/win32/SlotWatchdog.cpp
    src/infrastructure/hook/win32/Trampolines.hpp
    src/infrastructure/hook/win32/Trampolines.cpp
  >
)
target_include_directories(arkan_relay_infrastructure PUBLIC src)
target_include_directories(arkan_relay_infrastructure PRIVATE
  src/infrastructure/hook      
)
target_link_libraries(arkan_relay_infrastructure
  PUBLIC
    arkan_relay_application
    Boost::asio
  PRIVATE
    Boost::filesystem
    Boost::system
    spdlog::spdlog
    tomlplusplus::tomlplusplus
    Threads::Threads
)
if(WIN32)
  target_link_libraries(arkan_relay_infrastructure PUBLIC ws2_32)
  target_compile_definitions(arkan_relay_infrastructure PUBLIC _WIN32_WINNT=0x0601)
endif()

# -----------------------------------------------------------------------------
# Windows-specific: the final DLL adapter
# -----------------------------------------------------------------------------
if(WIN32)
  add_library(arkan_relay SHARED
    src/adapters/outbound/dll/DllMain.cpp
  )
  target_include_directories(arkan_relay PRIVATE src)

  # IMPORTANT:
  # - arkan_relay_infrastructure is STATIC, so its external deps
  #   must also be linked at the final link step of the DLL.
  target_link_libraries(arkan_relay PRIVATE
    arkan_relay_infrastructure
  )

  set_target_properties(arkan_relay PROPERTIES OUTPUT_NAME "ArkanRelay")
  target_compile_definitions(arkan_relay PRIVATE _WIN32_WINNT=0x0601)
endif()

# -----------------------------------------------------------------------------
# spdlog header-only vs compiled-lib switches
# -----------------------------------------------------------------------------
if(WIN32)
  if(MSVC)
    target_compile_options(arkan_relay_infrastructure PRIVATE "/USPDLOG_HEADER_ONLY" "/USPDLOG_COMPILED_LIB")
    target_compile_options(arkan_relay               PRIVATE "/USPDLOG_HEADER_ONLY" "/USPDLOG_COMPILED_LIB")
  else()
    target_compile_options(arkan_relay_infrastructure PRIVATE "-USPDLOG_HEADER_ONLY" "-USPDLOG_COMPILED_LIB")
    target_compile_options(arkan_relay               PRIVATE "-USPDLOG_HEADER_ONLY" "-USPDLOG_COMPILED_LIB")
  endif()
endif()

# -----------------------------------------------------------------------------
# Tests (GoogleTest)
# -----------------------------------------------------------------------------
include(CTest)

if(ARKAN_BUILD_TESTS AND BUILD_TESTING)
  find_package(GTest CONFIG QUIET)

  if(NOT GTest_FOUND)
    message(STATUS "GTest not found via CONFIG; using FetchContent (requires internet).")
    include(FetchContent)
    FetchContent_Declare(googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    # When using static CRT in MSVC, GoogleTest needs this toggle to avoid mixing runtimes
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  enable_testing()
  include(GoogleTest)

  add_executable(arkan_relay_test_config tests/test_config.cpp)
  target_link_libraries(arkan_relay_test_config PRIVATE
    arkan_relay_infrastructure
    GTest::gtest_main
  )
  if(WIN32)
    target_link_libraries(arkan_relay_test_config PRIVATE ws2_32)
    target_compile_definitions(arkan_relay_test_config PRIVATE _WIN32_WINNT=0x0601)
  endif()
  gtest_discover_tests(arkan_relay_test_config)

  add_executable(arkan_relay_test_logger tests/test_logger.cpp)
  target_link_libraries(arkan_relay_test_logger PRIVATE
    arkan_relay_infrastructure
    GTest::gtest_main
  )
  if(WIN32)
    target_link_libraries(arkan_relay_test_logger PRIVATE ws2_32)
    target_compile_definitions(arkan_relay_test_logger PRIVATE _WIN32_WINNT=0x0601)
  endif()
  gtest_discover_tests(arkan_relay_test_logger)

  add_executable(arkan_relay_test_link tests/test_link_asio.cpp)
  target_link_libraries(arkan_relay_test_link PRIVATE
    arkan_relay_infrastructure
    GTest::gtest_main
  )
  if(WIN32)
    target_link_libraries(arkan_relay_test_link PRIVATE ws2_32)
    target_compile_definitions(arkan_relay_test_link PRIVATE _WIN32_WINNT=0x0601)
  endif()
  gtest_discover_tests(arkan_relay_test_link)

  if(WIN32)
    add_executable(arkan_relay_test_hook_win32 tests/test_hook_win32.cpp)
    target_link_libraries(arkan_relay_test_hook_win32 PRIVATE
      arkan_relay_infrastructure
      GTest::gtest_main
      ws2_32
    )
    target_compile_definitions(arkan_relay_test_hook_win32 PRIVATE _WIN32_WINNT=0x0601)
    gtest_discover_tests(arkan_relay_test_hook_win32)
  endif()
endif()
