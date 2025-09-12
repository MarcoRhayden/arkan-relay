Param(
  [string]$Config = "Debug",
  [string]$BuildDir = "build",
  [string]$Generator = "Visual Studio 17 2022"
)

$ErrorActionPreference = 'Stop'
$extraArgs = @('-DCMAKE_EXPORT_COMPILE_COMMANDS=ON')

# Check cmake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
  Write-Error 'CMake not found. Please install it and ensure it is in PATH.'
  exit 1
}

# vcpkg toolchain (optional)
if ($env:VCPKG_ROOT -and (Test-Path $env:VCPKG_ROOT)) {
  $toolchain = Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake'
  $extraArgs += @("-DCMAKE_TOOLCHAIN_FILE=$toolchain")
}
if (-not $env:VCPKG_ROOT) {
  Write-Warning 'VCPKG_ROOT not set - build may fail if dependencies are missing.'
}

# Configure & Build
$buildPath = Join-Path (Get-Location) $BuildDir

Write-Host ("Configuring project with {0}..." -f $Generator)
& cmake -S . -B $buildPath -G $Generator @extraArgs

Write-Host ("Building ({0})..." -f $Config)
& cmake --build $buildPath --config $Config

Write-Host ("Build completed in '{0}' (config: {1})" -f $buildPath, $Config)
