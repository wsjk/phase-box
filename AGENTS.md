# Phase Box: Agent Development Guide & Repo Rules

This file is automatically loaded into agent context. It provides immediate ground truth to prevent wasteful codebase exploration.

---

## ⚡ Ground Rules & Architecture

1. **Active Stack vs. Unbuilt Prototype:**
   - **ACTIVE PRODUCTION CODE:** Centered on `DeviceApp.hpp`, `PhaseBoxState.hpp`, `MacroEngine.hpp`, `SimControls.hpp`, and `PresetSession.hpp`. This is what is compiled by RP2040 firmware (`firmware/`), native simulator (`src/native_sim_main.cpp`), and 146 host unit tests (`tests/`).
   - **DO NOT TOUCH `include/core/` OR `src/main.cpp`:** These are unbuilt legacy prototypes. Do NOT grep them, import them, or edit them when extending features or fixing bugs.
2. **Dual-Core Workload Split (RP2040 Firmware):**
   - **Core 0 (Real-Time Control & MIDI Engine):** Quadrature encoder decoding, switch debouncing, ADC0 expression sampling, 60 Hz engine advance, `MidiChangeFilter` bandwidth enforcement, dual MIDI streaming (UART0 31,250 baud on GP0 + TinyUSB), and 30 FPS OLED frame rendering into lock-free `SeqlockBuffer`.
   - **Core 1 (Async Display & Screen Saver):** Exclusively owns `i2c0` SSD1306 (GP4 SDA, GP5 SCL); reads frames from `SeqlockBuffer` and updates panel without stalling Core 0. Manages `ScreenSaver` contrast dimming (dims to 0x01 after 30s of inactivity).
   - **Never perform blocking I/O, flash erase/writes, or I2C display drawing on Core 0.**
3. **MIDI Bandwidth Rule:**
   - Serial DIN MIDI runs at 31,250 baud (~3,125 B/s). Always route CC messages through `MidiChangeFilter` (sends on value change + periodic 1s refresh) rather than blasting raw CCs every cycle.
4. **Default MIDI Target:**
   - Desktop simulator routes CoreMIDI directly to `"FRASIL FM-1"` (configurable via `$PHASE_BOX_MIDI_DEST="name"`; `"virtual"` creates a DAW virtual port).

---

## 🗺️ File Map & Quick Navigation

| Task / Domain | Target Files | Key Symbols |
| :--- | :--- | :--- |
| **Board Pinout & Detents** | `include/hal/pinout.hpp` | `ENC_A`, `ENC_B`, `ENC_SW`, `FS_TAP`, `FS_MUTATE`, `EXPR_PEDAL` |
| **Menu Pages P1–P11** | `include/SimControls.hpp` | `simui::adjust()`, `simui::page_name()`, `simui::footer()`, `preview_curve()` |
| **Synthesizer State** | `include/PhaseBoxState.hpp` | `PhaseBoxState`, `advance()`, `evaluateMacros()`, `frozen`, `bypassed` |
| **Oscillator & Waveforms**| `include/PhaseOscillator.hpp`<br/>`src/PhaseOscillator.cpp` | `WaveShape` (`SINE`, `SAW`, `SQUARE`, `TRIANGLE`), `update()` |
| **Macro Engine & Divisions**| `include/MacroEngine.hpp` | `MacroDestination` (`scale`, `offset`, `clock_mult`), `evaluate()` |
| **Turing Shift Register** | `include/TuringMachine.hpp` | `TuringMachine`, `mutation_prob`, `step()` |
| **Presets & Auto-Save** | `include/PresetSession.hpp`<br/>`firmware/include/PresetManager.hpp` | `PresetSession`, `update()` (2s auto-save), hold-to-save (1s), slots 1–8 |
| **Hardware Driver Integration**| `include/DeviceApp.hpp`<br/>`firmware/src/firmware_main.cpp` | `DeviceApp`, `poll_inputs()`, `frame()`, `DualMidiOut` |
| **Simulator Main** | `src/native_sim_main.cpp` | Raylib loop, CoreMIDI dispatch, keyboard shortcuts |
| **Unit Test Suite** | `tests/test_device_app.cpp`<br/>`tests/test_sim_controls.cpp`, etc. | 146 GoogleTest cases across 37 suites |

---

## 🛠️ Verification & Fast Build Commands

* **Run all 146 Host Tests (400ms):**
  ```bash
  cmake -S tests -B build-test -DCMAKE_POLICY_VERSION_MINIMUM=3.5 && cmake --build build-test -j4 && ./build-test/run_host_tests
  ```
* **Build Desktop Simulator:**
  ```bash
  cmake -B build-native -S . && cmake --build build-native -j4
  ```
* **Build RP2040 Firmware (.uf2):**
  ```bash
  export PATH="/Applications/CMake.app/Contents/bin:/Users/ww/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin:$PATH"
  export PICO_SDK_PATH="/Users/ww/tools/pico-sdk"
  cmake -S firmware -B firmware/build -DPICO_SDK_PATH="$PICO_SDK_PATH" && cmake --build firmware/build -j4
  ```
