#!/usr/bin/env bash
set -euo pipefail

CONFIG="${1:-Debug}"
BUILD_DIR="${2:-build}"

./scripts/build.sh "${CONFIG}" "${BUILD_DIR}"

# Run tests (GoogleTest / CTest)
if [[ -f "${BUILD_DIR}/CTestTestfile.cmake" ]]; then
  ctest --test-dir "${BUILD_DIR}" --output-on-failure -C "${CONFIG}"
else
  # fallback
  (cd "${BUILD_DIR}" && ctest --output-on-failure -C "${CONFIG}")
fi

echo "✅ Tests completed (config: ${CONFIG})"
