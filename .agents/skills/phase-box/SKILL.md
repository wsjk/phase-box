---
name: phase-box
description: >-
  Development runbook and architectural guide for the Phase Box RP2040 video synth
  modulation engine firmware. Use this skill whenever building, testing, modifying,
  or debugging code in the phase-box repository.
---

# Phase Box Development Skill

This skill provides fast, token-efficient procedures for building, testing, and developing the **Phase Box** firmware.

---

## Environment & Toolchain Quick Reference

* **CMake Binary**: `/Applications/CMake.app/Contents/bin/cmake`
* **Pico SDK Path**: `/Users/ww/tools/pico-sdk`
* **ARM GCC Cross-Compiler**: `/Users/ww/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin/arm-none-eabi-gcc`

Always prefix compilation commands with the required PATH:
```bash
export PATH="/Applications/CMake.app/Contents/bin:/Users/ww/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin:$PATH"
export PICO_SDK_PATH="/Users/ww/tools/pico-sdk"
```

---

## Common Workflows

### 1. Host Unit Testing (GoogleTest on macOS)

Use for fast simulation, math verification, and regression testing without hardware.

```bash
# Helper script:
./.agents/skills/phase-box/scripts/build_host.sh

# Or manual execution:
/Applications/CMake.app/Contents/bin/cmake -B build-host -DBUILD_HOST_TESTS=ON
/Applications/CMake.app/Contents/bin/cmake --build build-host -j4
./build-host/tests/run_host_tests
```

### 2. RP2040 Target Firmware Build (`.uf2`)

Use for cross-compiling the deployable firmware for Raspberry Pi Pico.

```bash
# Helper script:
./.agents/skills/phase-box/scripts/build_pico.sh

# Or manual execution:
export PATH="/Applications/CMake.app/Contents/bin:/Users/ww/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin:$PATH"
cmake -B build-pico -DBUILD_HOST_TESTS=OFF -DPICO_SDK_PATH=/Users/ww/tools/pico-sdk
cmake --build build-pico -j4
```
Output artifacts in `build-pico/`:
* `phase_box.uf2`: Ready for drag-and-drop USB flashing.
* `phase_box.elf`: For Picoprobe/GDB debugging.

---

## Architectural Rules & Dual-Core Division

See detailed notes in [Architecture Reference](./references/architecture.md).

1. **Hardware Decoupling (HAL)**:
   * Keep all algorithm math (`PhaseLFO`, `ClockManager`, `MidiRouter`, `ModulationEngine`) independent of hardware headers.
   * Provide mock HAL implementations (`MockHalMidi`, `MockHalGpio`) for host tests.
2. **Core 0 Workload (Real-Time 1000 Hz)**:
   * Runs via hardware repeating timer interrupt (`add_repeating_timer_us(-1000, ...)`).
   * Executes `Core0Task::step(uint32_t current_time_us)` at 1000 Hz.
   * Handles UART MIDI output (31250 baud on GP0), external clock tick polling (GP1), and footswitches.
   * **Never** perform blocking I/O, display drawing, or flash erase/writes on Core 0.
3. **Core 1 Workload (UI & Persistence)**:
   * Launched from Core 0 via `multicore_launch_core1(core1_entry)`.
   * Throttled OLED rendering (30 FPS max over I2C at 1 MHz Fast-Mode Plus).
   * Rotary encoder debouncing and menu navigation state machine.
   * Preset serialization and SPI NOR flash writes on explicit user request ("Hold-to-Save").

---

## Completed Roadmap & Implementation Status

All roadmap phases from the Handover Document and ADR are fully implemented and verified:

* **Step 1 (Fix Assertions)**: Vector index access in tests resolved.
* **Step 2 (Host Test Harness)**: GoogleTest suite established with 24 passing unit tests across 8 suites.
* **Step 3 (Core 0 Real-Time Engine)**: 1000 Hz hardware repeating timer interrupt, 4-channel LFO calculation, UART MIDI at 31250 baud on GP0, 24 PPQN external MIDI clock fallback, and tap tempo / phase reset.
* **Step 4 (Core 1 UI & Drivers)**: SSD1306 128x32 OLED driver at 1 MHz I2C, 4-state rotary menu machine (Telemetry, Param Edit, Hold-to-Save, Tap Sync), and SPI NOR flash preset storage.
* **Macro Engine & ADC**: GP26 analog expression pedal reading, bipolar weighted matrix scaling across channels, and nested LFO cross-modulation.

---

## Extension & Maintenance Guide

### Adding New Waveforms
1. Add new enum value to `Waveform` in `include/core/phase_lfo.hpp`.
2. Implement phase mapping in `PhaseLFO::update()`.
3. Add name string to `UIController::waveform_to_string()`.
4. Add ASCII graphic pattern to `UIController::render_telemetry_view()`.

### Adjusting Hardware Pinout
Update default pins in `PicoHalMidi`, `PicoHalGpio`, `PicoHalAdc`, or `PicoHalDisplay`:
- MIDI TX: GP0, RX: GP1
- Encoder: GP2 (A), GP3 (B), GP6 (Switch)
- Footswitches: GP10 (Tap/Reset), GP11 (Mutate)
- OLED I2C0: GP4 (SDA), GP5 (SCL)
- Expression ADC0: GP26

