Param(
  [string]$Config = "Debug",
  [string]$BuildDir = "build",
  [string]$Generator = ""   # ex.: "Ninja" or "Visual Studio 17 2022"
)

$ErrorActionPreference = "Stop"
$extraArgs = @("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON")

# ---------------------------------------------------------------------------
# Check cmake
# ---------------------------------------------------------------------------
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
  Write-Error "❌ CMake not found. Please install it and ensure it's in PATH."
  exit 1
}

# ---------------------------------------------------------------------------
# Prefer Ninja if no generator specified
# ---------------------------------------------------------------------------
if (-not $Generator) {
  if (Get-Command ninja -ErrorAction SilentlyContinue) {
    $Generator = "Ninja"
  }
}
if ($Generator) {
  $extraArgs += @("-G", $Generator)
}

# ---------------------------------------------------------------------------
# vcpkg toolchain
# ---------------------------------------------------------------------------
if ($env:VCPKG_ROOT -and (Test-Path $env:VCPKG_ROOT)) {
  $toolchain = Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"
  $extraArgs += @("-DCMAKE_TOOLCHAIN_FILE=$toolchain")
} else {
  Write-Warning "⚠️ VCPKG_ROOT not set — build may fail if dependencies are missing."
}

# ---------------------------------------------------------------------------
# Configure & Build
# ---------------------------------------------------------------------------
$buildPath = Join-Path (Get-Location) $BuildDir

Write-Host "🔧 Configuring project..."
cmake -S . -B $buildPath @extraArgs

Write-Host "🛠️ Building ($Config)..."
cmake --build $buildPath --config $Config

Write-Host "✅ Build completed in '$buildPath' (config: $Config)"
