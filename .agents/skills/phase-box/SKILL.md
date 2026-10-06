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
   * Preset serialization and LittleFS flash writes on explicit user request ("Hold-to-Save").

---

## Next Implementation Task: Step 4 Runbook (Core 1 GUI & Drivers)

When implementing Step 4:
1. **OLED Display Driver**:
   * Implement SSD1306 128x32 I2C driver (I2C0, GP4 SDA, GP5 SCL).
   * Enforce delta-time check (33 ms interval = ~30 FPS).
2. **Rotary Encoder State Machine**:
   * Poll encoder delta (`read_encoder_delta()`) and button (`read_button(ButtonId::EncoderSwitch)`).
   * Implement 4 UI states:
     - State 1: Live Telemetry View
     - State 2: Parameter Edit Mode
     - State 3: Hold-to-Save Preset (1 second hold threshold)
     - State 4: Tap Tempo / Phase Reset sync display
3. **SPI Flash Preset Storage**:
   * Implement `HalFlash` using LittleFS or raw flash sector erase/write (`hardware/flash.h`).
   * Restrict all writes strictly to Core 1.
