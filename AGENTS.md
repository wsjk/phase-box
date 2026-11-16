# Phase Box: Agent Guide & Repo Rules

## Ground Rules

1. **Active code:** `DeviceApp.hpp`, `PhaseBoxState.hpp`, `MacroEngine.hpp`, `SimControls.hpp`, `PresetSession.hpp`. Built by the RP2040 firmware (`firmware/`), the simulator (`src/native_sim_main.cpp`) and the host tests (`tests/`, 186 tests in 42 suites).
   **DO NOT TOUCH `include/core/` OR `src/main.cpp`:** unbuilt legacy prototypes. Do not grep, import or edit them.
2. **Dual-core firmware:** Core 0 runs encoders, debounce, the 60 Hz engine, `MidiChangeFilter`, MIDI out (UART0 GP0 + TinyUSB) and renders OLED frames into `SeqlockBuffer`. Core 1 alone owns `i2c0` (SSD1306, GP4/GP5) and the `ScreenSaver`.
   **Never do blocking I/O or I2C drawing on Core 0.** Only exception: preset saves, which go through `FlashPresetStorage` -> `PicoFlash` (details in the firmware skill).
3. **MIDI bandwidth:** DIN runs at 31,250 baud. Always send CCs through `MidiChangeFilter`, never raw every cycle.
4. **Simulator MIDI target:** CoreMIDI `"FRASIL FM-1"` by default; `$PHASE_BOX_MIDI_DEST="name"` overrides, `"virtual"` makes a DAW port.
5. **Docs check and release notes (every commit or PR):**
   - Before committing code, build-file or hardware changes, run `ls docs/*.md`, then read `README.md` and every listed file. In the same commit, fix any text the change makes wrong (controls, pages, pinout, wiring, build commands, test counts). Also check `AGENTS.md`, affected `.agents/skills/` files and READMEs in the changed folder.
   - No changelog file: each merge to `main` is released automatically (`.github/workflows/release.yml`) with the merged PR titles as notes. Write the PR title as a release-note line. Label `minor` for a new feature or behaviour change, `major` for a breaking one (e.g. a preset format old presets cannot load); otherwise patch.
   - Fix only what the change makes wrong. Full audit: the `docs-drift-check` skill, if installed.
   - Say in the PR description which docs you updated, or that none needed it.

## File Map

| Domain | Files | Key symbols |
| :--- | :--- | :--- |
| Pinout & detents | `include/hal/pinout.hpp` | `UART_TX/RX`, `ENC_A/B/SW`, `EXP_ENC_A/B/SW`, `FS_TAP/MUTATE/BYPASS` |
| Menu pages P1–P12 | `include/SimControls.hpp` | `simui::adjust()`, `page_name()`, `footer()`, `preview_curve()` |
| Synth state | `include/PhaseBoxState.hpp` | `advance()`, `evaluateMacros()`, `frozen`, `bypassed` |
| Oscillator | `include/PhaseOscillator.hpp`, `src/PhaseOscillator.cpp` | `WaveShape`, `update()` |
| Macro engine | `include/MacroEngine.hpp` | `MacroDestination` (`scale`, `offset`, `clock_mult`), `evaluate()` |
| Turing register | `include/TuringMachine.hpp` | `mutation_prob`, `step()` |
| Presets | `include/PresetSession.hpp`, `firmware/include/PresetManager.hpp` | 2 s auto-save, 1 s hold-to-save, slots 1–8 |
| Flash storage | `include/FlashPresetStorage.hpp`, `firmware/include/PicoFlash.hpp` | 2 sectors/slot, append + CRC, `flash_safe_execute` |
| Device driver | `include/DeviceApp.hpp`, `firmware/src/firmware_main.cpp` | `poll_inputs()`, `frame()`, `DualMidiOut` |
| Simulator UI | `src/native_sim_main.cpp`, `include/SimLayout.hpp`, `include/SimPot.hpp` | Raylib loop, `KnobDrag` |
| CoreMIDI | `include/MidiRouter.hpp`, `include/CoreMidiBackend.hpp` | `midi_route::Router`, `DEFAULT_DESTINATION` |
| OLED | `include/OledRenderer.hpp`, `include/SimFont.hpp` | `render()`, 128×32 / 128×64 |
| Tests | `tests/test_device_app.cpp`, `tests/test_sim_layout.cpp`, ... | GoogleTest |
| Main board (KiCad) | `hardware/main-board/` | see its `README.md` |

## Skills (`.agents/skills/<name>/SKILL.md`)

Read the matching one before working in its area: `phase-box` (master runbook, builds), `phase-box-firmware` (Pico SDK, pinout, Core 1, flash), `phase-box-sim` (Raylib, CoreMIDI), `phase-box-dsp-macro` (60 Hz pipeline, waveforms, Turing, P1–P12), `phase-box-presets` (PBX1 v3, CRC-32, saving), `raylib-hardware-sim`, `skill-creator`.

## Build & Test

```bash
./.agents/skills/phase-box/scripts/build_host.sh     # host tests (~430 ms)
cmake -B build-native -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5 && cmake --build build-native -j4   # simulator
./.agents/skills/phase-box/scripts/build_pico.sh     # firmware .uf2 (manual toolchain steps: phase-box-firmware skill)
```
