# Phase Box: Agent Development Guide & Repo Rules

This file is automatically loaded into agent context. It provides immediate ground truth to prevent wasteful codebase exploration.

---

## ⚡ Ground Rules & Architecture

1. **Active Stack vs. Unbuilt Prototype:**
   - **ACTIVE PRODUCTION CODE:** Centered on `DeviceApp.hpp`, `PhaseBoxState.hpp`, `MacroEngine.hpp`, `SimControls.hpp`, and `PresetSession.hpp`. This is what is compiled by RP2040 firmware (`firmware/`), native simulator (`src/native_sim_main.cpp`), and 186 host unit tests across 42 test suites (`tests/`).
   - **DO NOT TOUCH `include/core/` OR `src/main.cpp`:** These are unbuilt legacy prototypes. Do NOT grep them, import them, or edit them when extending features or fixing bugs.
2. **Dual-Core Workload Split (RP2040 Firmware):**
   - **Core 0 (Real-Time Control & MIDI Engine):** Quadrature decoding of both encoders (value + expression), switch debouncing, 60 Hz engine advance, `MidiChangeFilter` bandwidth enforcement, dual MIDI streaming (UART0 31,250 baud on GP0 + TinyUSB), and 30 FPS OLED frame rendering into lock-free `SeqlockBuffer`.
   - **Core 1 (Async Display & Screen Saver):** Exclusively owns `i2c0` SSD1306 (GP4 SDA, GP5 SCL); reads frames from `SeqlockBuffer` and updates panel without stalling Core 0. Manages `ScreenSaver` contrast dimming (dims to 0x01 after 30s of inactivity).
   - **Never perform blocking I/O or I2C display drawing on Core 0.** The one exception is preset saving: flash erase/program stalls *both* cores whichever core runs it (XIP is off), so it goes only through `FlashPresetStorage` -> `PicoFlash` (`flash_safe_execute`; Core 1 calls `flash_safe_execute_core_init()`). It costs ~1 ms per save and ~45 ms for the erase every 8th save of a slot.
3. **MIDI Bandwidth Rule:**
   - Serial DIN MIDI runs at 31,250 baud (~3,125 B/s). Always route CC messages through `MidiChangeFilter` (sends on value change + periodic 1s refresh) rather than blasting raw CCs every cycle.
4. **Default MIDI Target:**
   - Desktop simulator routes CoreMIDI directly to `"FRASIL FM-1"` (configurable via `$PHASE_BOX_MIDI_DEST="name"`; `"virtual"` creates a DAW virtual port).

---

## 🗺️ File Map & Quick Navigation

| Task / Domain | Target Files | Key Symbols |
| :--- | :--- | :--- |
| **Board Pinout & Detents** | `include/hal/pinout.hpp` | `MIDI_TX`, `MIDI_RX`, `ENC_A`, `ENC_B`, `ENC_SW`, `EXP_ENC_A`, `EXP_ENC_B`, `EXP_ENC_SW`, `FS_TAP`, `FS_MUTATE`, `FS_BYPASS` |
| **Menu Pages P1–P12** | `include/SimControls.hpp` | `simui::adjust()`, `simui::page_name()`, `simui::footer()`, `preview_curve()` |
| **Synthesizer State** | `include/PhaseBoxState.hpp` | `PhaseBoxState`, `advance()`, `evaluateMacros()`, `frozen`, `bypassed` |
| **Oscillator & Waveforms**| `include/PhaseOscillator.hpp`<br/>`src/PhaseOscillator.cpp` | `WaveShape` (`SINE`, `SAW`, `SQUARE`, `TRIANGLE`), `update()` |
| **Macro Engine & Divisions**| `include/MacroEngine.hpp` | `MacroDestination` (`scale`, `offset`, `clock_mult`), `evaluate()` |
| **Turing Shift Register** | `include/TuringMachine.hpp` | `TuringMachine`, `mutation_prob`, `step()` |
| **Presets & Auto-Save** | `include/PresetSession.hpp`<br/>`firmware/include/PresetManager.hpp` | `PresetSession`, `update()` (2s auto-save), hold-to-save (1s), slots 1–8 |
| **Flash Preset Storage (device)** | `include/FlashPresetStorage.hpp`<br/>`firmware/include/PicoFlash.hpp` | `FlashPresetStorage` (2 sectors/slot, append + CRC), `FlashDevice`, `PicoFlash` (`flash_safe_execute`) |
| **Hardware Driver Integration**| `include/DeviceApp.hpp`<br/>`firmware/src/firmware_main.cpp` | `DeviceApp`, `poll_inputs()` (press-and-turn slot, TAP+MUTATE 2 s reset/undo), `frame()`, `DualMidiOut` |
| **Simulator Main & Stompbox UI** | `src/native_sim_main.cpp`<br/>`include/SimLayout.hpp`<br/>`include/SimPot.hpp` | Raylib loop, stompbox enclosure geometry, `KnobDrag`, expression encoder drag/scroll |
| **CoreMIDI Router & Fallback** | `include/MidiRouter.hpp`<br/>`include/CoreMidiBackend.hpp` | `midi_route::Router`, `DEFAULT_DESTINATION` (`"FRASIL FM-1"`), auto-reconnect |
| **OLED Renderer & Font** | `include/OledRenderer.hpp`<br/>`include/SimFont.hpp` | `OledRenderer::render()`, 128×32 / 128×64 modes, custom bitmap font |
| **Unit Test Suite** | `tests/test_device_app.cpp`<br/>`tests/test_sim_layout.cpp`, etc. | 186 GoogleTest cases across 42 suites |

---

## 🧠 Agent Skills & Task Runbooks (`.agents/skills/`)

Specialized procedures, architectural constraints, and step-by-step recipes are maintained in `.agents/skills/`. Agents (Claude, Gemini, Antigravity) should read the relevant `SKILL.md` when assigned tasks in these domains:

| Domain / Task | Skill Directory | Key Topics |
| :--- | :--- | :--- |
| **Master Runbook** | [`.agents/skills/phase-box/SKILL.md`](.agents/skills/phase-box/SKILL.md) | Overall workflow, build commands, dual-core rules |
| **RP2040 Firmware** | [`.agents/skills/phase-box-firmware/SKILL.md`](.agents/skills/phase-box-firmware/SKILL.md) | Pico SDK, cross-compiler, pinout, Core 1 OLED & screensaver |
| **Simulator & Raylib UI** | [`.agents/skills/phase-box-sim/SKILL.md`](.agents/skills/phase-box-sim/SKILL.md) | Stompbox geometry, knob/expression-encoder drag, CoreMIDI, Raylib guide |
| **DSP, Waveforms & Menu** | [`.agents/skills/phase-box-dsp-macro/SKILL.md`](.agents/skills/phase-box-dsp-macro/SKILL.md) | 60 Hz pipeline, waveform recipes, Turing LFSR, P1–P12 |
| **Presets & Storage** | [`.agents/skills/phase-box-presets/SKILL.md`](.agents/skills/phase-box-presets/SKILL.md) | PBX1 v3 binary format, CRC-32, 2s auto-save, hold-to-save |
| **Generic Hardware Sim** | [`.agents/skills/raylib-hardware-sim/SKILL.md`](.agents/skills/raylib-hardware-sim/SKILL.md) | Reusable template for building stompbox/hardware UIs with Raylib |
| **Skill Creator** | [`.agents/skills/skill-creator/SKILL.md`](.agents/skills/skill-creator/SKILL.md) | Scaffolding and runbook for designing token-efficient agent skills |

---

## 🛠️ Verification & Fast Build Commands

* **Run all 186 Host Tests (430ms):**
  ```bash
  cmake -S tests -B build-test -DCMAKE_POLICY_VERSION_MINIMUM=3.5 && cmake --build build-test -j4 && ./build-test/run_host_tests
  # Or helper script: ./.agents/skills/phase-box/scripts/build_host.sh
  ```
* **Build Desktop Simulator:**
  ```bash
  cmake -B build-native -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5 && cmake --build build-native -j4
  ```
* **Build RP2040 Firmware (.uf2):**
  ```bash
  # Helper script (auto-detects SDK and toolchain paths):
  ./.agents/skills/phase-box/scripts/build_pico.sh

  # Or manual execution:
  export PATH="/Applications/CMake.app/Contents/bin:/Users/ww/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin:$PATH"
  export PICO_SDK_PATH="/Users/ww/src/pico-sdk"
  cmake -S firmware -B firmware/build -DPICO_SDK_PATH="$PICO_SDK_PATH" && cmake --build firmware/build -j4
  ```
