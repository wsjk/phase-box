---
name: phase-box-firmware
description: >-
  RP2040 firmware build, flash, and hardware integration runbook for Phase Box.
  Use when compiling the Raspberry Pi Pico UF2 binary, modifying GPIO pinouts,
  verifying dual-core execution (Core 0 real-time engine vs. Core 1 async OLED),
  debugging TinyUSB MIDI or hardware UART serial, or troubleshooting physical controls.
---

# Phase Box RP2040 Firmware Skill

This skill provides step-by-step procedures for cross-compiling, debugging, and flashing the Phase Box RP2040 firmware onto Raspberry Pi Pico silicon.

---

## ⚡ Ground Rules for RP2040 Firmware

1. **Dual-Core Workload Split**:
   - **Core 0 (Real-Time Control & MIDI Engine)**:
     - High-frequency input polling: Quadrature encoder (GP2, GP3), debounced switches (GP6, GP10, GP11), ADC0 expression pedal (GP26 at 200 Hz).
     - TinyUSB service (`tud_task()`).
     - 60 Hz modulation engine step (`FRAME_US` = 16,667 µs).
     - `MidiChangeFilter` bandwidth enforcement (DIN MIDI limit: 3,125 B/s).
     - Dual MIDI output: UART0 (GP0 TX at 31,250 baud) + TinyUSB USB-MIDI.
     - 30 FPS OLED frame rendering into lock-free `SeqlockBuffer<FRAME_BYTES>`.
     - **NEVER perform blocking I/O, flash erase/writes, or I2C display drawing on Core 0.**
   - **Core 1 (Async Display & Screen Saver)**:
     - Exclusively owns `i2c0` SSD1306 display on GP4 (SDA) and GP5 (SCL).
     - Reads finished frames from `SeqlockBuffer` and pushes them to panel.
     - Runs `ScreenSaver` contrast dimming (dims to 0x01 after 30s of inactivity; restores to 0xCF on activity).
     - Never stalls or blocks Core 0.

2. **Decoupled Architecture (`DeviceApp`)**:
   - Hardware-specific pin reads and peripheral setup live strictly in `firmware/src/firmware_main.cpp`.
   - All state machine logic, parameter navigation (P1–P12), and OLED frame generation live in `DeviceApp.hpp`.
   - Any firmware feature must be verified first via host unit tests in `tests/test_device_app.cpp`.

---

## 🛠️ Build & Flash Workflows

### 1. Automated Firmware Build

Run the automated build script (automatically detects the ARM GCC toolchain and Pico SDK paths):

```bash
./.agents/skills/phase-box/scripts/build_pico.sh
```

### 2. Manual Firmware Compilation

```bash
export PATH="/Applications/CMake.app/Contents/bin:/Users/ww/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin:$PATH"
export PICO_SDK_PATH="/Users/ww/src/pico-sdk"

cmake -S firmware -B firmware/build -DPICO_SDK_PATH="$PICO_SDK_PATH"
cmake --build firmware/build -j4
```

### 3. Deploying UF2 to Pico

1. Unplug the Pico from USB.
2. Hold down the white **BOOTSEL** button on the Pico board and plug it into USB.
3. The board mounts as a mass storage volume named `RPI-RP2`.
4. Copy the compiled UF2 file:
   ```bash
   cp firmware/build/phase_box_firmware.uf2 /Volumes/RPI-RP2/
   ```
5. The Pico automatically reboots and launches Phase Box firmware.

---

## 🔍 Hardware Pinout Reference

See [pinout_and_hardware.md](./references/pinout_and_hardware.md) for full circuit schematics, pull-up specs, and detent settings.

| Signal | RP2040 Pin | Hardware Role |
| :--- | :--- | :--- |
| `UART0_TX` | **GP0** | 31,250 baud serial MIDI TX (connect via 220Ω resistor) |
| `UART0_RX` | **GP1** | Serial MIDI RX (reserved) |
| `ENC_A` | **GP2** | Rotary encoder phase A (pulled high, switch to GND) |
| `ENC_B` | **GP3** | Rotary encoder phase B (pulled high, switch to GND) |
| `OLED_SDA` | **GP4** | `i2c0` data line (SSD1306 address `0x3C`) |
| `OLED_SCL` | **GP5** | `i2c0` clock line (400 kHz standard, up to 1 MHz) |
| `ENC_SW` | **GP6** | Encoder pushbutton (pulled high, switch to GND) |
| `FS_TAP` | **GP10** | Footswitch 1: Tap tempo & phase reset |
| `FS_MUTATE` | **GP11** | Footswitch 2: Turing machine mutate trigger |
| `EXPR_PEDAL`| **GP26** (ADC0)| 0.0V–3.3V analog expression voltage (TRS tip) |

---

## 🩺 Hardware Bringup & Troubleshooting Checklist

1. **Screen Blank**:
   - Check if panel is 32 or 64 rows: recompile with `-DPHASEBOX_OLED_HEIGHT=64` if using a 128×64 display.
   - Check I2C address: standard is `0x3C`, some eBay/Amazon modules use `0x3D`.
   - Confirm GP4 (SDA) and GP5 (SCL) connections.
2. **Display Inverted or Upside Down**:
   - In `include/hal/pico_hal_display.hpp`, update `PicoHalDisplay::init()` commands: segment remap (`0xA0` vs `0xA1`) and scan direction (`0xC0` vs `0xC8`).
3. **Encoder Steps Backward or Double-Ticks**:
   - Backward: Change `#define ENC_REVERSED 1` in `include/hal/pinout.hpp`.
   - Detents: Change `ENC_TRANSITIONS_PER_DETENT` (usually 4 or 2) in `include/hal/pinout.hpp`.
4. **USB MIDI Not Recognized**:
   - Confirm `firmware/src/usb_descriptors.c` and `firmware/include/tusb_config.h` are compiling. Check macOS Audio MIDI Setup or run `system_profiler SPUSBDataType`.
5. **Expression Pedal Jitter**:
   - Adjust EMA smoothing constant `ALPHA` in `include/hal/hal_adc.hpp`.
