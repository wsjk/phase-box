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
     - High-frequency input polling: Quadrature encoder (GP2, GP3), debounced switches (GP6 encoder, GP9 freeze, GP10 tap, GP11 mutate, GP12 bypass), quadrature expression encoder (GP7, GP8).
     - Preset saves to flash via `FlashPresetStorage` -> `PicoFlash` (`flash_safe_execute`; pauses both cores ~1 ms, ~45 ms every 8th save of a slot). Core 1 calls `flash_safe_execute_core_init()` first; never write flash any other way.
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
| `UART_TX` | **GP0** | 31,250 baud serial MIDI TX (3.3 V wiring: see pinout_and_hardware.md) |
| `UART_RX` | **GP1** | Serial MIDI RX (reserved) |
| `ENC_A` | **GP2** | Rotary encoder phase A (pulled high, switch to GND) |
| `ENC_B` | **GP3** | Rotary encoder phase B (pulled high, switch to GND) |
| `OLED_SDA` | **GP4** | `i2c0` data line (SSD1306 address `0x3C`) |
| `OLED_SCL` | **GP5** | `i2c0` clock line (400 kHz standard, up to 1 MHz) |
| `ENC_SW` | **GP6** | Encoder pushbutton (pulled high, switch to GND) |
| `EXP_ENC_A` | **GP7** | Expression encoder phase A (pulled high, switch to GND) |
| `EXP_ENC_B` | **GP8** | Expression encoder phase B (pulled high, switch to GND) |
| `EXP_ENC_SW` | **GP9** | Expression encoder pushbutton: freeze on / off |
| `FS_TAP` | **GP10** | Footswitch 1: Tap tempo & phase reset (hold with FS 2 for 2 s: reset / undo) |
| `FS_MUTATE` | **GP11** | Footswitch 2: Turing machine mutate trigger |
| `FS_BYPASS` | **GP12** | Footswitch 3: bypass on / off |

---

## 🩺 Hardware Bringup & Troubleshooting Checklist

1. **Screen Blank**:
   - Check if panel is 32 or 64 rows: the default is 64; recompile with `-DPHASEBOX_OLED_HEIGHT=32` if using a 128×32 display.
   - Check I2C address: standard is `0x3C`, some eBay/Amazon modules use `0x3D`.
   - Confirm GP4 (SDA) and GP5 (SCL) connections.
2. **Display Inverted or Upside Down**:
   - In `include/hal/pico_hal_display.hpp`, update `PicoHalDisplay::init()` commands: segment remap (`0xA0` vs `0xA1`) and scan direction (`0xC0` vs `0xC8`).
3. **Encoder Steps Backward or Double-Ticks**:
   - Backward: Set `ENC_REVERSED` (value encoder) or `EXP_ENC_REVERSED` (expression encoder) to `true` in `include/hal/pinout.hpp`.
   - Detents: Change `ENC_TRANSITIONS_PER_DETENT` (usually 4 or 2) in `include/hal/pinout.hpp`.
4. **USB MIDI Not Recognized**:
   - Confirm `firmware/src/usb_descriptors.c` and `firmware/include/tusb_config.h` are compiling. Check macOS Audio MIDI Setup or run `system_profiler SPUSBDataType`.
5. **Expression Encoder Does Nothing**:
   - Confirm GP7 (A) and GP8 (B) wiring; the common pin goes to GND (internal pull-ups are enabled).
