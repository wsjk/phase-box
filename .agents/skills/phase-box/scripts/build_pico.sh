#!/usr/bin/env bash
set -euo pipefail

# Navigate to repo root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
cd "${REPO_ROOT}"

# Setup environment variables & toolchain
export PICO_SDK_PATH="${PICO_SDK_PATH:-/Users/ww/tools/pico-sdk}"
export PATH="/Applications/CMake.app/Contents/bin:/Users/ww/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin:${PATH}"

echo "==> Configuring RP2040 Target Build (Pico SDK)..."
cmake -B build-pico -DBUILD_HOST_TESTS=OFF -DPICO_SDK_PATH="${PICO_SDK_PATH}"

echo "==> Compiling RP2040 Firmware..."
cmake --build build-pico -j4

echo "==> RP2040 Firmware Generated:"
ls -lh build-pico/phase_box.uf2 build-pico/phase_box.elf
