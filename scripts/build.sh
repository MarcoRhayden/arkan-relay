#!/usr/bin/env bash
set -euo pipefail

CONFIG="${1:-Debug}"        # Debug/Release/RelWithDebInfo/MinSizeRel
BUILD_DIR="${2:-build}"     # build directory
GEN="${GENERATOR:-}"        # override: export GENERATOR="Ninja"

EXTRA_ARGS=("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON")

# Prefer Ninja se existir
if command -v ninja >/dev/null 2>&1; then
  GEN="${GEN:-Ninja}"
fi
if [[ -n "${GEN}" ]]; then
  EXTRA_ARGS+=("-G" "${GEN}")
fi

# Homebrew (macOS)
if command -v brew >/dev/null 2>&1; then
  BREW_PREFIX="$(brew --prefix)"
  EXTRA_ARGS+=("-DCMAKE_PREFIX_PATH=${BREW_PREFIX}")
fi

# vcpkg
if [[ -n "${VCPKG_ROOT:-}" && -d "$VCPKG_ROOT" ]]; then
  EXTRA_ARGS+=("-DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
fi

# Configure & Build
cmake -S . -B "${BUILD_DIR}" "${EXTRA_ARGS[@]}"
cmake --build "${BUILD_DIR}" --config "${CONFIG}" -j

echo "✅ Build completed in '${BUILD_DIR}' (config: ${CONFIG})"
