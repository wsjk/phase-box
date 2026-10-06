#!/usr/bin/env bash
set -euo pipefail

# Navigate to repo root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
cd "${REPO_ROOT}"

echo "==> Configuring Host Tests (macOS / GoogleTest)..."
/Applications/CMake.app/Contents/bin/cmake -B build-host -DBUILD_HOST_TESTS=ON

echo "==> Building Host Tests..."
/Applications/CMake.app/Contents/bin/cmake --build build-host -j4

echo "==> Running Host Unit Tests..."
./build-host/tests/run_host_tests
