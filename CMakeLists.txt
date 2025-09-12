cmake_minimum_required(VERSION 3.26)
project(arkan_relay LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# Habilite warnings úteis
if(MSVC)
add_compile_options(/W4 /permissive-)
else()
add_compile_options(-Wall -Wextra -Wpedantic)
endif()


# If using vcpkg, both below should resolve under manifest (vcpkg.json)
find_package(spdlog CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)


add_library(arkan_relay_core STATIC
src/domain/Settings.hpp
src/infrastructure/Config_Toml.hpp
src/infrastructure/Config_Toml.cpp
src/infrastructure/Logger_Spdlog.hpp
src/infrastructure/Logger_Spdlog.cpp
)


target_include_directories(arkan_relay_core PUBLIC src)


target_link_libraries(arkan_relay_core PRIVATE
spdlog::spdlog
tomlplusplus::tomlplusplus
)


add_executable(arkan_relay_cli
src/interface/CliMain.cpp
)


target_link_libraries(arkan_relay_cli PRIVATE arkan_relay_core)