# Arkan Relay

> C++ bridge/relay with **Boost.Asio**, **spdlog** and **toml++**, designed with **Clean Architecture**.  
> Provides resilient TCP connectivity, binary framing (R/S/K), configurable ports, and structured logging.  
> **Academic/educational use in a controlled environment.**

## Features
- 🚦 **Resilient TCP client** (reconnect + keepalive ‘K’) with Boost.Asio
- 📦 **Binary framing**: `kind('R'|'S'|'K') + u16 length + payload`
- ⚙️ **Config TOML** with self-generated patterns
- 🧾 **Logging** with rotation (app and socket) via spdlog
- 🧱 **Clean Architecture**: domain / application / infrastructure / interface

## Tech Stack
- C++20, CMake
- Boost.Asio, spdlog, toml++

## Getting Started
```bash
# vcpkg (manifest)
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
./build/arkan_relay_cli
