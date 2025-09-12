Param(
  [string]$Config = "Debug",
  [string]$BuildDir = "build",
  [string]$Generator = ""   # ex.: "Ninja" or "Visual Studio 17 2022"
)

$ErrorActionPreference = "Stop"
$extraArgs = @("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON")

# Prefer Ninja
if (-not $Generator) {
  if (Get-Command ninja -ErrorAction SilentlyContinue) {
    $Generator = "Ninja"
  }
}
if ($Generator) {
  $extraArgs += @("-G", $Generator)
}

# vcpkg toolchain
if ($env:VCPKG_ROOT -and (Test-Path $env:VCPKG_ROOT)) {
  $toolchain = Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"
  $extraArgs += @("-DCMAKE_TOOLCHAIN_FILE=$toolchain")
}

# Configure & Build
cmake -S . -B $BuildDir @extraArgs
cmake --build $BuildDir --config $Config

Write-Host "✅ Build completed in '$BuildDir' (config: $Config)"
