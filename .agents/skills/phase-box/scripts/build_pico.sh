#!/usr/bin/env bash
set -euo pipefail

# Navigate to repo root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
cd "${REPO_ROOT}"

# Add known CMake locations if cmake is not found in PATH
if ! command -v cmake >/dev/null 2>&1; then
    if [ -x "/Applications/CMake.app/Contents/bin/cmake" ]; then
        export PATH="/Applications/CMake.app/Contents/bin:$PATH"
    fi
fi

# Add known ARM GCC toolchain to PATH if arm-none-eabi-gcc is not found
if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    for toolchain in \
        "/Users/ww/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin" \
        "$HOME/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin"; do
        if [ -d "$toolchain" ]; then
            export PATH="$toolchain:$PATH"
            break
        fi
    done
fi

# Discover Pico SDK location if not explicitly provided
if [ -z "${PICO_SDK_PATH:-}" ]; then
    for sdk_candidate in \
        "/Users/ww/src/pico-sdk" \
        "/Users/ww/tools/pico-sdk" \
        "$HOME/src/pico-sdk" \
        "$HOME/tools/pico-sdk"; do
        if [ -d "$sdk_candidate" ]; then
            export PICO_SDK_PATH="$sdk_candidate"
            break
        fi
    done
fi

: "${PICO_SDK_PATH:?Set PICO_SDK_PATH to your pico-sdk checkout}"
CMAKE="${CMAKE:-cmake}"

echo "==> Using PICO_SDK_PATH=${PICO_SDK_PATH}"
echo "==> Configuring RP2040 target build (Pico SDK)..."
"${CMAKE}" -S firmware -B firmware/build -DPICO_SDK_PATH="${PICO_SDK_PATH}"

echo "==> Compiling RP2040 firmware..."
"${CMAKE}" --build firmware/build -j4

echo "==> RP2040 firmware generated:"
ls -lh firmware/build/*.uf2 firmware/build/*.elf
