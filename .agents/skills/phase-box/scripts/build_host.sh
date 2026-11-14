#!/usr/bin/env bash
set -euo pipefail

# Navigate to repo root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
cd "${REPO_ROOT}"

if ! command -v cmake >/dev/null 2>&1; then
    if [ -x "/Applications/CMake.app/Contents/bin/cmake" ]; then
        export PATH="/Applications/CMake.app/Contents/bin:$PATH"
    fi
fi

CMAKE="${CMAKE:-cmake}"
BUILD_DIR="${BUILD_DIR:-build-test}"

echo "==> Configuring host tests (${BUILD_DIR})..."
"${CMAKE}" -S tests -B "${BUILD_DIR}" -DCMAKE_POLICY_VERSION_MINIMUM=3.5

echo "==> Building host tests..."
"${CMAKE}" --build "${BUILD_DIR}" -j4

echo "==> Running host unit tests..."
"./${BUILD_DIR}/run_host_tests"
