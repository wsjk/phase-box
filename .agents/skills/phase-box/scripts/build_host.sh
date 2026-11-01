#!/usr/bin/env bash
set -euo pipefail

# Navigate to repo root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
cd "${REPO_ROOT}"

CMAKE="${CMAKE:-cmake}"

echo "==> Configuring host tests (GoogleTest is fetched on first configure; needs network)..."
"${CMAKE}" -S tests -B build-host -DCMAKE_POLICY_VERSION_MINIMUM=3.5

echo "==> Building host tests..."
"${CMAKE}" --build build-host -j4

echo "==> Running host unit tests..."
./build-host/run_host_tests
