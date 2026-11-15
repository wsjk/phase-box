---
name: phase-box
description: >-
  Development runbook and architectural guide for the Phase Box RP2040 video synth
  modulation engine firmware. Use this skill whenever building, testing, modifying,
  or debugging code in the phase-box repository.
---

# Phase Box Development Skill

This skill provides fast, token-efficient procedures for building, testing, and developing the **Phase Box** firmware and simulator.

---

## Environment & Toolchain Quick Reference

* **CMake Binary**: `/Applications/CMake.app/Contents/bin/cmake`
* **Pico SDK Path**: `/Users/ww/src/pico-sdk` (or `/Users/ww/tools/pico-sdk`)
* **ARM GCC Cross-Compiler**: `/Users/ww/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin/arm-none-eabi-gcc`

Compilation environment variables (automatically handled by the helper scripts):
```bash
export PATH="/Applications/CMake.app/Contents/bin:/Users/ww/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin:$PATH"
export PICO_SDK_PATH="/Users/ww/src/pico-sdk"
```

---

## Common Workflows

### 1. Host Unit Testing (GoogleTest on macOS)

Fastest simulation, math verification, and regression testing without hardware (170 tests across 40 suites, ~430ms runtime):

```bash
# Helper script (auto-detects tools and runs tests):
./.agents/skills/phase-box/scripts/build_host.sh

# Or manual execution:
cmake -S tests -B build-test -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build-test -j4
./build-test/run_host_tests
```

### 2. Desktop Native Simulator (Raylib + CoreMIDI)

```bash
cmake -B build-native -S .
cmake --build build-native -j4
./build-native/phase_box_native_sim
```
*Features realistic stompbox layout, interactive mouse knob dragging (circular or vertical) and wheel detents, expression encoder dragging, footswitch clicks, and CoreMIDI routing directly to `"FRASIL FM-1"` (override with `$PHASE_BOX_MIDI_DEST="name"`; `"virtual"` creates DAW virtual source).*

### 3. RP2040 Target Firmware Build (`.uf2`)

Cross-compiling the deployable firmware for Raspberry Pi Pico:

```bash
# Helper script (auto-detects toolchain and SDK paths):
./.agents/skills/phase-box/scripts/build_pico.sh

# Or manual execution:
export PATH="/Applications/CMake.app/Contents/bin:/Users/ww/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin:$PATH"
export PICO_SDK_PATH="/Users/ww/src/pico-sdk"
cmake -S firmware -B firmware/build -DPICO_SDK_PATH="$PICO_SDK_PATH"
cmake --build firmware/build -j4
```
Output artifacts in `firmware/build/`:
* `phase_box_firmware.uf2`: Ready for drag-and-drop USB flashing.
* `phase_box_firmware.elf`: For Picoprobe/GDB debugging.

---

## Architectural Rules & Dual-Core Division

See detailed notes in [Architecture Reference](./references/architecture.md).

> [!IMPORTANT]
> **Production Stack vs. Legacy Prototypes:**
> The active codebase is centered on `include/DeviceApp.hpp`, `include/PhaseBoxState.hpp`, `include/MacroEngine.hpp`, `include/SimControls.hpp`, and `include/PresetSession.hpp`.
> **Do not edit `include/core/` or `src/main.cpp`** — they are unbuilt legacy prototypes.

1. **Hardware Decoupling (`DeviceApp`)**:
   * All physical inputs (both quadrature encoders, buttons, footswitches), state advancing, parameter editing (pages P1–P12), preset auto-save, and OLED rendering are encapsulated in `DeviceApp.hpp`.
   * This allows complete zero-hardware verification in `tests/test_device_app.cpp` using mock inputs.
2. **Core 0 Workload (Real-Time Control & MIDI Engine)**:
   * Polls inputs: Quadrature encoder (GP2, GP3), debounced switches (GP6, GP10, GP11), expression encoder (GP7, GP8).
   * Runs TinyUSB tasks (`tud_task()`).
   * Steps modulation engine at 60 Hz (`DeviceApp::FRAME_US` = 16,667 µs).
   * Filters MIDI CC messages via `MidiChangeFilter` (sends on value change + periodic 1s refresh to respect the 3,125 B/s DIN MIDI bandwidth).
   * Fans out CCs to UART0 (31,250 baud on GP0) and TinyUSB USB-MIDI.
   * Renders OLED frame at 30 FPS into lock-free `SeqlockBuffer<FRAME_BYTES>`.
   * **Never perform blocking I/O, display I2C drawing, or flash erase/writes on Core 0.**
3. **Core 1 Workload (Async Display & Screen Saver)**:
   * Launched from Core 0 via `multicore_launch_core1(core1_entry)`.
   * Exclusively owns `i2c0` SSD1306 (GP4 SDA, GP5 SCL) at 400 kHz / 1 MHz.
   * Reads completed frames from `SeqlockBuffer` and copies them to the display without stalling Core 0.
   * Runs `ScreenSaver` contrast dimming (dims to 0x01 after 30s of inactivity; wakes on user activity).

---

## Extension & Maintenance Guide

### Adding New Waveforms
1. Add new enum value to `WaveShape` in `include/PhaseOscillator.hpp`.
2. Implement normalized waveform math in `PhaseOscillator::update()` in `src/PhaseOscillator.cpp`.
3. Add name string to `wave_names` in `include/SimControls.hpp`.

### Adjusting Hardware Pinout
Update constants in `include/hal/pinout.hpp` (single source of truth):
- MIDI TX: GP0, RX: GP1
- Encoder: GP2 (A), GP3 (B), GP6 (Switch)
- Footswitches: GP10 (Tap/Reset), GP11 (Mutate)
- OLED I2C0: GP4 (SDA), GP5 (SCL)
- Expression encoder: GP7 (A), GP8 (B)
- Detent count & direction: `ENC_TRANSITIONS_PER_DETENT` (both encoders), `ENC_REVERSED`, `EXP_ENC_REVERSED`
