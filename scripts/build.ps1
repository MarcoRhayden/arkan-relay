Param(
  [string]$Config   = "Debug",
  [string]$BuildDir = "build",
  [string]$Generator = "Visual Studio 17 2022",
  [string]$Triplet  = "x86-windows"
)

$ErrorActionPreference = 'Stop'
$extraArgs = @('-DCMAKE_EXPORT_COMPILE_COMMANDS=ON')

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

  Write-Host "-- Running vcpkg install"
  & "$env:VCPKG_ROOT\vcpkg.exe" install --triplet $Triplet
  Write-Host "-- Running vcpkg install - done"
} else {
  Write-Warning 'VCPKG_ROOT not set - builds may fail if dependencies are missing.'
}

$arch = ""
if ($Triplet -like "x86-*") { $arch = "Win32" }
elseif ($Triplet -like "x64-*") { $arch = "x64" }

$buildPath = Join-Path (Get-Location) $BuildDir
New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

$archDisplay = "<none>"
if ($arch -ne "") { $archDisplay = $arch }

Write-Host ("Configuring project with {0} (Triplet: {1}, -A {2})..." -f $Generator, $Triplet, $archDisplay)

if ($arch -ne "") {
  & cmake -S . -B $buildPath -G $Generator -A $arch @extraArgs
} else {
  & cmake -S . -B $buildPath -G $Generator @extraArgs
}

Write-Host ("Building ({0})..." -f $Config)
& cmake --build $buildPath --config $Config

Write-Host ("Build completed in '{0}' (config: {1})" -f $buildPath, $Config)
