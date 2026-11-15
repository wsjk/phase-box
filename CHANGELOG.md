# Changelog

Notable changes to Phase Box: firmware, simulator, hardware and docs. Newest first.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). There are no
version numbers yet, so merged work is grouped by merge date.

Add each change under **Unreleased** in the same commit as the change, in one of these groups:
Added, Changed, Fixed, Removed. Write it for someone using or building the device, and end it
with the PR number.

## Unreleased

### Added

- `CHANGELOG.md`, and a rule in `AGENTS.md`: every change checks `README.md` and `docs/*.md`
  and adds a changelog line.

## 2026-11-14

### Added

- Expression encoder push switch (GP9) toggles freeze. A third footswitch (GP12) toggles
  bypass. (#30)
- Press and turn the value encoder to change preset slot 1–8. Hold TAP and MUTATE for 2 s to
  reset to defaults, or to undo the last reset if nothing changed since. (#30)
- Presets saved to flash on the device (`FlashPresetStorage`): last 64 KB, two sectors per
  slot, append-only records with CRC, so a power cut keeps the last good save. (#30)
- Simulator: BYPASS footswitch and click-to-freeze on the expression encoder. (#30)
- `PHASE_BOX_SCREENSHOT=file.png` makes the simulator save one screenshot and quit. (#28)
- Simulator screenshot in the README. (#27)
- Agent skills for firmware, simulator, DSP and presets, and a skill-scaffolding skill.
  (#25, #26)

### Changed

- The analog expression pedal is replaced by a second encoder (GP7/GP8). It sets expression
  from 0 to 1 in 20 clicks and scales every destination's depth from 0.5x to 1x. (#28)
- `include/hal/pinout.hpp` is the only pin map, including the UART pins. (#29, #30)
- The simulator build needs `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` on CMake 4. (#30)
- Claude Code worktrees are git-ignored. (#31)

### Fixed

- Wiring notes: 3.3 V MIDI out resistors (33 Ω and 10 Ω), an optocoupler for MIDI in on GP1,
  and external pull-ups for long footswitch leads. The README no longer suggests 1 MHz I²C for
  the SSD1306. (#29)

### Removed

- The ADC expression input and `hal_adc` from the firmware (#28). The unused `PicoHalGpio`
  (#29).

## 2026-11-12

### Changed

- The simulator looks like the physical stompbox. (#24)

## 2026-11-07

### Added

- Offset page. (#23)
- `AGENTS.md` (with `CLAUDE.md` linked to it) and updated docs, including the
  multi-destination CC workflow in the user guide. (#22, #23)

## 2026-11-05

### Added

- Desktop simulator sends MIDI over CoreMIDI to a named device. Freeze and bypass. (#21)

## 2026-11-03

### Added

- P10/P11 pages, per-destination MIDI channel and CC, and shape, phase, PM and Turing-machine
  values sent as MIDI. (#19)
- Auto-save, hold-to-save and preset slots. A parameter reset button. (#19)
- A firmware build target for the layered stack (OLED menu, encoder, footswitches). (#19)

### Fixed

- Clock sync, MIDI send rate, HAL interfaces, the preset format and the MIDI router. (#19)

### Removed

- Build output from the repo. (#20)

## 2026-10-30

### Added

- Clock divisions and multipliers per macro destination (polyrhythms). (#18)

## 2026-10-09

### Added

- MIDI out, a second LFO, the Turing machine, the multi-destination macro engine with an OLED
  workflow, an ASCII visualiser and the Pico firmware. (#9–#17)

## 2026-10-08

### Added

- Simulator script and Pico build. (#5–#8)

## 2026-10-05

### Added

- First version: Core 0 real-time loop with UART MIDI, Core 1 SSD1306 OLED, encoder menu, flash
  presets, macro engine, host tests and agent docs. (#1–#4)
