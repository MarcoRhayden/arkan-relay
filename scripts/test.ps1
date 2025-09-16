Param(
  [string]$Config = "Debug",
  [string]$BuildDir = "build",
  [string]$Generator = "Visual Studio 17 2022",
  [string]$Triplet = "x86-windows"
)

$ErrorActionPreference = "Stop"

# Build
.\scripts\build.ps1 -Config $Config -BuildDir $BuildDir -Generator $Generator -Triplet $Triplet

# Tests (CTest)
ctest --test-dir $BuildDir --output-on-failure -C $Config

Write-Host ("✅ Tests completed (config: {0}, triplet: {1})" -f $Config, $Triplet)
