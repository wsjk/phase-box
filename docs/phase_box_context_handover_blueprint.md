# Phase Box Firmware: Architecture Context & Handover Blueprint

This document contains the complete state, architecture specifications, and implementation code for the remaining phases of the **Phase Box** project. Use this file as a zero-context prompt to continue development in any agent or environment.

---

## 1. System Overview & Active State

* **Target Hardware:** Raspberry Pi Pico (RP2040 micro-controller)
* **Architecture:** Asymmetric Dual-Core Operational Model
  * **Core 0:** Real-time control loop, input polling (quadrature encoder GP2/3, debounce GP6/9/10/11/12, expression encoder GP7/8), 60 Hz DSP engine (Phase Oscillator, Turing-machine mutation logic, multi-destination polyrhythmic macro scaling, MidiChangeFilter bandwidth limiting), and dual MIDI transmission (UART0 31,250 baud on GP0 + TinyUSB USB-MIDI). Renders OLED frames to lock-free `SeqlockBuffer`.
  * **Core 1:** Async display loop. Exclusively owns `i2c0` SSD1306 (GP4 SDA, GP5 SCL), reads rendered frames from `SeqlockBuffer`, and runs automatic screen burn-in contrast dimming via `ScreenSaver`.
* **Storage:** State serialization format `PBX1` (version 3 with CRC32) with an automated 2-second idle "dirty-state" auto-save watcher in `PresetSession`. (`FlashPresetStorage` in the last 64 KB of flash on RP2040; `FilePresetStorage` on desktop).
* **Build Configuration:** CMake toolchain cross-compiling via `arm-none-eabi-gcc` and `pico-sdk`. Tested and generates `phase_box_firmware.uf2`.

---

## 2. Completed Functionality

1. **Polyrhythmic Macro Engine (`include/MacroEngine.hpp`):** Independent clock multipliers (`clock_mult`), phase accumulators, scale, and offset controls per destination.
2. **USB MIDI Class Compliance (`firmware/src/usb_descriptors.c` & `firmware/include/tusb_config.h`):** Configured TinyUSB streaming interface enabling plug-and-play USB MIDI CC output to DAWs/hardware.
3. **Preset State Serialization & Session (`firmware/include/PresetManager.hpp`, `include/PresetSession.hpp`):** Safe, atomic serialization with CRC32, dirty-timer auto-save (2s idle), hold-to-save (1s hold), and 8 preset slots.
4. **OLED Screen Burn-In Protection (`include/ScreenSaver.hpp`, `firmware/src/firmware_main.cpp`):** Automatic inactivity timer (30 seconds) dimming SSD1306 contrast to minimum (`0x01`), with immediate restore to `0xCF` on user activity.
5. **Desktop Simulator & Stompbox UI (`src/native_sim_main.cpp`, `include/SimLayout.hpp`, `include/SimPot.hpp`):** Raylib native desktop simulation with interactive mouse knob dragging, detent scrolling, expression encoder drag/scroll and click-to-freeze, three footswitches (TAP, MUTATE, BYPASS), and macOS CoreMIDI routing directly to `FRASIL FM-1`.
6. **Host Test Suite (`tests/`):** 186 GoogleTest unit test cases across 41 test suites covering DSP math, hardware abstraction layers, DeviceApp simulation, preset session persistence, SimLayout geometry, and CoreMIDI routing.

---

## 3. Operational Architecture & Core 1 ScreenSaver

Small OLED displays (e.g., SSD1306 over I2C) are susceptible to static element burn-in during long live performances. Phase Box includes an automatic inactivity timer and contrast dimming system.

### Specifications
* **Inactivity Window:** 30 seconds of zero encoder or button activity (`ScreenSaver::TIMEOUT_MS = 30000`).
* **Dimming Mechanism:** Contrast command register (`0x81`) dimmed to minimum value (`0x01`) on SSD1306 via `PicoHalDisplay::set_contrast()`.
* **Wake Mechanism:** Any encoder tick or button press updates `g_last_activity_ms`, which instantly restores contrast register to default (`0xCF`) and returns to normal UI rendering.

### Core 1 Implementation (`firmware/src/firmware_main.cpp`)

```cpp
void core1_entry() {
    namespace pins = phasebox::hal::pins;
    phasebox::hal::PicoHalDisplay display(i2c0, pins::OLED_SDA, pins::OLED_SCL,
                                          phasebox::hal::PicoHalDisplay::DEFAULT_I2C_ADDR, OLED_W, OLED_H);
    display.init();
    display.set_contrast(ScreenSaver::CONTRAST_NORMAL);

    ScreenSaver saver;
    static uint8_t frame[FRAME_BYTES];
    uint32_t last_seq = 0xFFFFFFFFu;
    for (;;) {
        uint32_t seq;
        if (g_shared_frame.read(frame, &seq) && seq != last_seq) {   // only send frames that changed
            display.load_frame(frame, sizeof(frame));
            display.update();
            last_seq = seq;
        }
        if (saver.update(ms_since_boot(), g_last_activity_ms.load(std::memory_order_relaxed))) {
            display.set_contrast(saver.contrast());
        }
        sleep_ms(5);
    }
}
```

---

## 4. Deployment & Flashing

Firmware compiles to UF2 for drag-and-drop flashing to the RP2040.

### Build Commands
```bash
# Automated script (auto-detects Pico SDK and ARM toolchain paths):
./.agents/skills/phase-box/scripts/build_pico.sh

# Or manual execution:
export PATH="/Applications/CMake.app/Contents/bin:/Users/ww/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin:$PATH"
export PICO_SDK_PATH="/Users/ww/src/pico-sdk"
cmake -S firmware -B firmware/build -DPICO_SDK_PATH="$PICO_SDK_PATH"
cmake --build firmware/build -j4
```

### Flashing Instructions
1. Disconnect the RP2040 board from USB power.
2. Hold the **BOOTSEL** button on the Pico and reconnect the USB cable.
3. Copy `phase_box_firmware.uf2` to the mounted `RPI-RP2` drive:
```bash
cp firmware/build/phase_box_firmware.uf2 /Volumes/RPI-RP2/
```