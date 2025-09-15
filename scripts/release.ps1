Param(
  [string]$BuildDir = "build_release_x86",
  [string]$Generator = "Visual Studio 17 2022",
  [string]$Triplet  = "x86-windows-static",
  [string]$Config   = "Release",
  [string]$OutDir   = "dist_x86"
)

$ErrorActionPreference = 'Stop'
$extraArgs = @(
  '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
  '-DARKAN_RELEASE=ON',
  '-DARKAN_BUILD_TESTS=OFF'
)

# Check cmake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
  Write-Error 'CMake not found. Install it and ensure it is in PATH.'
  exit 1
}

# vcpkg toolchain + manifests
if ($env:VCPKG_ROOT -and (Test-Path $env:VCPKG_ROOT)) {
  $toolchain = Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake'
  $extraArgs += @("-DCMAKE_TOOLCHAIN_FILE=$toolchain")
  $extraArgs += @("-DVCPKG_FEATURE_FLAGS=manifests")
  $extraArgs += @("-DVCPKG_TARGET_TRIPLET=$Triplet")

  Write-Host "-- Running vcpkg install (triplet: $Triplet)"
  & "$env:VCPKG_ROOT\vcpkg.exe" install --triplet $Triplet
  Write-Host "-- vcpkg install done"
} else {
  Write-Warning 'VCPKG_ROOT not set - builds may fail if dependencies are missing.'
}

# Recreate build dir
$buildPath = Join-Path (Get-Location) $BuildDir
if (Test-Path $buildPath) { Remove-Item -Recurse -Force $buildPath }
New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

Write-Host ("[Release x86] Configuring project with {0} (Triplet: {1}, -A Win32)..." -f $Generator, $Triplet)

& cmake -S . -B $buildPath -G $Generator -A Win32 @extraArgs

Write-Host ("[Release x86] Building ({0})..." -f $Config)
& cmake --build $buildPath --config $Config

# Output dir
$distPath = Join-Path (Get-Location) $OutDir
New-Item -ItemType Directory -Force -Path $distPath | Out-Null

# Copy only final DLL (rename to .asi)
$dllPath = Join-Path $buildPath "$Config\ArkanRelay.dll"
if (-not (Test-Path $dllPath)) {
  Write-Error "Build succeeded but DLL not found: $dllPath"
  exit 1
}

$asiPath = Join-Path $distPath "ArkanRelay.asi"
Copy-Item $dllPath $asiPath -Force

Write-Host "[Release x86] Build finished! ASI ready at: $asiPath"
