#!/usr/bin/env bash
set -euo pipefail

# Navigate to repo root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
cd "${REPO_ROOT}"

# The Pico SDK location must be provided by the caller, e.g.
#   PICO_SDK_PATH=$HOME/tools/pico-sdk ./build_pico.sh
: "${PICO_SDK_PATH:?Set PICO_SDK_PATH to your pico-sdk checkout}"
CMAKE="${CMAKE:-cmake}"

echo "==> Configuring RP2040 target build (Pico SDK)..."
"${CMAKE}" -S firmware -B firmware/build -DPICO_SDK_PATH="${PICO_SDK_PATH}"

echo "==> Compiling RP2040 firmware..."
"${CMAKE}" --build firmware/build -j4

echo "==> RP2040 firmware generated:"
ls -lh firmware/build/*.uf2 firmware/build/*.elf
