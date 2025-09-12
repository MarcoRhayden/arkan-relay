Param(
  [string]$Config = "Debug",
  [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

.\scripts\build.ps1 -Config $Config -BuildDir $BuildDir

# Run tests (GoogleTest / CTest)
ctest --test-dir $BuildDir --output-on-failure -C $Config

Write-Host "✅ Tests completed (config: $Config)"
