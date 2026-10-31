# Phase Box Firmware: Architecture Context & Handover Blueprint

This document contains the complete state, architecture specifications, and implementation code for the remaining phases of the **Phase Box** project. Use this file as a zero-context prompt to continue development in any agent or environment.

---

## 1. System Overview & Active State

* **Target Hardware:** Raspberry Pi Pico (RP2040 micro-controller)
* **Architecture:** Asymmetric Dual-Core Operational Model
  * **Core 0:** Real-time DSP engine (Phase Oscillator, Turing-machine mutation logic, multi-destination polyrhythmic macro scaling, and TinyUSB MIDI class-compliant packet transmission).
  * **Core 1:** UI state machine, rotary encoder polling, OLED display housekeeping, and screen burn-in protection.
* **Storage:** LittleFS flash persistence with an automated 2-second idle "dirty-state" auto-save watcher.
* **Build Configuration:** CMake toolchain referencing `pico-sdk` and cross-compiling via `arm-none-eabi-gcc`.

---

## 2. Completed Functionality

1. **Polyrhythmic Macro Engine (`include/MacroEngine.hpp`):** Independent clock multipliers (`clock_mult`), phase accumulators, scale, and offset controls per destination.
2. **USB MIDI Class Compliance (`firmware/src/usb_descriptors.c` & `firmware/include/tusb_config.h`):** Configured TinyUSB streaming interface enabling plug-and-play USB MIDI CC output to DAWs/hardware.
3. **Flash Storage (`firmware/include/PresetManager.hpp`):** LittleFS state serialization for non-volatile preset persistence.

---

## 3. Phase 4 Implementation Plan: OLED Screen Burn-In Protection

Small OLED displays (e.g., SSD1306 over I2C) are susceptible to static element burn-in during long live performances. Phase 4 adds an automatic inactivity timer and contrast dimming system.

### Specifications
* **Inactivity Window:** 30 seconds of zero encoder or button activity.
* **Dimming Mechanism:** Write `0x81` (Set Contrast) command with minimum value (`0x01`) to SSD1306.
* **Wake Mechanism:** Any encoder tick or button press instantly restores contrast register to default (`0xCF`) and returns to normal UI rendering.

### Reference Core 1 Implementation (`firmware/src/screensaver.cpp`)

```cpp
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define OLED_I2C_ADDR 0x3C
#define OLED_CMD_SET_CONTRAST 0x81

void oled_set_contrast(i2c_inst_t* i2c, uint8_t contrast_val) {
    uint8_t cmd[2] = { 0x00, OLED_CMD_SET_CONTRAST };
    i2c_write_blocking(i2c, OLED_I2C_ADDR, cmd, 2, false);
    
    uint8_t val[2] = { 0x00, contrast_val };
    i2c_write_blocking(i2c, OLED_I2C_ADDR, val, 2, false);
}

void core1_ui_loop() {
    absolute_time_t last_activity = get_absolute_time();
    bool is_dimmed = false;

    while (true) {
        bool user_input_detected = false; // Set to true when encoder/button interrupts fire

        if (user_input_detected) {
            last_activity = get_absolute_time();
            if (is_dimmed) {
                oled_set_contrast(i2c1, 0xCF); // Restore brightness
                is_dimmed = false;
            }
        }

        // Check for 30-second timeout (30,000,000 microseconds)
        if (!is_dimmed && absolute_time_diff_us(last_activity, get_absolute_time()) > 30000000ULL) {
            oled_set_contrast(i2c1, 0x01); // Dim display
            is_dimmed = true;
        }

        sleep_ms(16); // ~60 FPS update loop
    }
}
```

---

## 4. Phase 5 Execution Plan: Deployment & Flashing

Once Phase 4 is integrated, compile the final firmware and deploy the UF2 binary.

### Build Commands
```bash
# Clean and configure build directory
cd firmware
rm -rf build && mkdir build && cd build

# Configure CMake with cross-compiler toolchain
cmake .. \
  -DPICO_SDK_PATH=/Users/ww/src/pico-sdk \
  -DCMAKE_C_COMPILER=/Users/ww/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin/arm-none-eabi-gcc \
  -DCMAKE_CXX_COMPILER=/Users/ww/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin/arm-none-eabi-g++ \
  -DCMAKE_BUILD_TYPE=Release

# Build project
cmake --build . -j$(sysctl -n hw.ncpu)
```

### Flashing Instructions
1. Disconnect the RP2040 board from USB power.
2. Hold the **BOOTSEL** button on the Pico and reconnect the USB cable.
3. Copy `phase_box_firmware.uf2` to the mounted `RPI-RP2` drive:
```bash
cp firmware/build/phase_box_firmware.uf2 /Volumes/RPI-RP2/
```