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
  add_compile_options(/W4 /permissive- /utf-8)
  add_definitions(-DUNICODE -D_UNICODE)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Dependencies (via vcpkg manifest)
# ---------------------------------------------------------------------------
find_package(spdlog CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)

# Use Boost CONFIG packages from vcpkg (avoid FindBoost/CMP0167)
find_package(boost_filesystem CONFIG REQUIRED)
find_package(boost_system CONFIG REQUIRED)
find_package(boost_asio CONFIG REQUIRED)

find_package(Threads REQUIRED)

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
  PUBLIC
    arkan_relay_application
    Boost::asio                        # headers (propaga include dirs)
  PRIVATE
    Boost::filesystem                  # lib
    Boost::system                      # lib (alguns toolchains ainda pedem)
    spdlog::spdlog
    tomlplusplus::tomlplusplus
    Threads::Threads
)

if(WIN32)
  # Hook Win32 (envio/recepção do cliente)
  target_sources(arkan_relay_infrastructure PRIVATE
    src/infrastructure/hook/Hook_Win32.hpp
    src/infrastructure/hook/Hook_Win32.cpp
  )
  # Winsock
  target_link_libraries(arkan_relay_infrastructure PUBLIC ws2_32)
  # Definição de target mínimo do Windows para Asio
  target_compile_definitions(arkan_relay_infrastructure PUBLIC _WIN32_WINNT=0x0601)
endif()

# ---------------------------------------------------------------------------
# Windows-specific (DLL adapter)
# ---------------------------------------------------------------------------
if(WIN32)
  add_library(arkan_relay SHARED
    src/adapters/outbound/dll/DllMain.cpp
  )
  target_include_directories(arkan_relay PRIVATE src)
  target_link_libraries(arkan_relay PRIVATE arkan_relay_infrastructure)
  set_target_properties(arkan_relay PROPERTIES OUTPUT_NAME "ArkanRelay")

  # Asio target define também na DLL (por segurança, quando incluir diretamente headers)
  target_compile_definitions(arkan_relay PRIVATE _WIN32_WINNT=0x0601)
endif()

# ------------------------------------------------------------------------------
# Tests (GoogleTest)
# ------------------------------------------------------------------------------
include(CTest)
option(ARKAN_BUILD_TESTS "Build unit tests" ON)

if(ARKAN_BUILD_TESTS AND BUILD_TESTING)
  find_package(GTest CONFIG QUIET)

  if(NOT GTest_FOUND)
    message(STATUS "GTest not found via CONFIG, falling back to FetchContent (needs internet).")
    include(FetchContent)
    FetchContent_Declare(googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  enable_testing()
  include(GoogleTest)

  # --- tests/test_config.cpp
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

  # --- tests/test_logger.cpp
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

  # --- tests/test_link_asio.cpp
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

  # --- tests/test_hook_win32.cpp
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