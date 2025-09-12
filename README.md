<p align="center">
  <img src="docs/img/openkore.png" alt="OpenKore" height="150" />
  &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
  <img src="docs/img/arkansoftware.png" alt="Arkan Software" height="150" />
</p>

# Arkan Relay

**Arkan Relay** is a modular C++20 relay that sits between the Ragnarok Online client and OpenKore, acting as a transparent bridge that intercepts, frames, and forwards game packets (TCP) to enable stable bot-side processing. The solution emphasizes Clean Architecture, with TOML-based configuration, spdlog-backed structured logging (file + rotation), and a GoogleTest/CTest suite. It is designed strictly for closed, lab-grade environments. This is not intended for use on third-party servers or in production contexts.


> This repository is being developed for study in a closed environment.

## 🧱 Architecture (Clean Architecture)

- **domain/**: pure models (e.g., `Settings`).
- **application/**: ports (interfaces) and orchestration services (e.g., `IConfigProvider`, `ILogger`, `Bootstrap`).
- **infrastructure/**: concrete implementations of the ports (TOML via toml++, filesystem via Boost.Filesystem, logging via spdlog).
- **adapters/**: edges of the system (CLI).

Dependency direction (only inward):

```
domain  ←  application  ←  infrastructure  ←  adapters (CLI)
```

### Folder layout

```
.
├─ arkan-relay.toml                 # configuration file (default)
├─ CMakeLists.txt
├─ scripts/
│  ├─ build.sh / build.ps1          # build (macOS/Linux and Windows)
│  └─ test.sh / test.ps1            # tests (macOS/Linux and Windows)
├─ src/
│  ├─ domain/
│  │  └─ Settings.hpp
│  ├─ application/
│  │  ├─ ports/
│  │  │  ├─ IConfigProvider.hpp
│  │  │  └─ ILogger.hpp
│  │  └─ services/
│  │     └─ Bootstrap.hpp
│  ├─ infrastructure/
│  │  ├─ config/
│  │  │  ├─ Config_Toml.hpp
│  │  │  └─ Config_Toml.cpp
│  │  └─ logging/
│  │     ├─ Logger_Spdlog.hpp
│  │     └─ Logger_Spdlog.cpp
│  └─ adapters/
│     └─ inbound/
│        └─ cli/
│           └─ CliMain.cpp
└─ tests/
   ├─ test_bootstrap.cpp
   ├─ test_config.cpp
   └─ test_logger.cpp
```

---

## ⚙️ Requirements

### macOS (Homebrew)
```bash
brew install cmake ninja boost spdlog tomlplusplus
```

### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build
# packages (if available on your distro)
sudo apt install -y libboost-all-dev libspdlog-dev
# toml++ may vary by distro; if missing, use vcpkg (below)
```

### Windows
- **Visual Studio 2022** (Desktop C++) or **MSVC Build Tools**
- **CMake** and (optional) **Ninja**
- Recommended: **vcpkg**

#### vcpkg (optional, all platforms)
```bash
git clone https://github.com/microsoft/vcpkg
./vcpkg/bootstrap-vcpkg.sh     # (Windows: .\vcpkg\bootstrap-vcpkg.bat)

# install dependencies
./vcpkg/vcpkg install boost-filesystem spdlog tomlplusplus

# help the scripts auto-detect vcpkg
export VCPKG_ROOT=/path/to/vcpkg     # PowerShell: $env:VCPKG_ROOT="C:\path\to\vcpkg"
```

> The project also auto-detects **Homebrew** on macOS through `CMAKE_PREFIX_PATH`.

---

## 🛠️ Build

Use the cross-platform scripts:

### macOS / Linux
```bash
chmod +x scripts/build.sh scripts/test.sh
./scripts/build.sh Debug     # or Release
```

### Windows (PowerShell)
```powershell
.\scripts\build.ps1 -Config Debug   # or Release
```

Manual build (alternative):
```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix)" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --config Debug
```

---

## 🧪 Tests

Run the tests (GoogleTest via CTest):

```bash
# macOS/Linux
./scripts/test.sh Debug

# Windows (PowerShell)
.\scripts\test.ps1 -Config Debug
```

Filter by name:
```bash
ctest --test-dir build -R ConfigToml --output-on-failure
```

---

## ▶️ Run the CLI

```bash
./build/arkan_relay_cli arkan-relay.toml
# or simply:
./build/arkan_relay_cli
# (if the file does not exist, it will be created with defaults)
```

---

## 🧩 Configuration (`arkan-relay.toml`)

**[general]**
- `ports` (array of integers): listening ports (e.g., `[6900, 6901, 6902]`).
- `showConsole` (bool): also output logs to the console.
- `saveLog` / `saveSocketLog` (bool): write logs to files for app/socket channels.

**[advanced]** *(optional)*
- `fnSeedAddr`, `fnChecksumAddr`, `fnSendAddr`, `fnRecvAddr`: hexadecimal addresses as strings (e.g., `"0x1234ABCD"`). Leave empty if unused.

**[logging]**
- `dir`: log directory (e.g., `"logs"`).
- `app`: application log filename.
- `socket`: socket channel log filename.

> **Log rotation:** 5 MB per file, 3 files per channel (see `Logger_Spdlog.cpp`).


## 💡 VS Code tips

- Extensions: **CMake Tools** and **C/C++** (ms-vscode.cpptools).
- Ensure IntelliSense uses `compile_commands.json`:
  ```json
  {
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "C_Cpp.default.compileCommands": "${workspaceFolder}/build/compile_commands.json",
    "C_Cpp.default.cppStandard": "c++20"
  }
  ```
- If you see false red squiggles on includes, reconfigure/clean:
  ```bash
  rm -rf build && ./scripts/build.sh
  ```
