---
name: phase-box-sim
description: >-
  Desktop simulator and CoreMIDI integration runbook for Phase Box.
  Use when developing, debugging, or running the native Raylib desktop simulator,
  modifying stompbox layout geometry (SimLayout/SimPot), tuning mouse/trackpad knob
  or treadle dragging, or adjusting CoreMIDI routing to FRASIL FM-1 and DAW virtual ports.
---

# Phase Box Desktop Simulator Skill

This skill provides comprehensive instructions for developing, testing, and running the **Phase Box native desktop simulator** (`src/native_sim_main.cpp`).

---

## 🚀 Quick Commands

### 1. Build Desktop Simulator

```bash
cmake -B build-native -S .
cmake --build build-native -j4
```

### 2. Run Simulator

```bash
# Default: sends CoreMIDI straight to "FRASIL FM-1"
./build-native/phase_box_native_sim

# Route to a DAW (creates virtual port "Phase Box Virtual Out"):
PHASE_BOX_MIDI_DEST=virtual ./build-native/phase_box_native_sim

# Route to a specific MIDI hardware destination:
PHASE_BOX_MIDI_DEST="My Synth" ./build-native/phase_box_native_sim

# List all available CoreMIDI destinations and sources:
./build-native/phase_box_native_sim --list-midi
```

---

## 🎛️ Stompbox UI & Interactive Controls

The simulator provides a 1:1 graphical model of the physical hardware enclosure.
* Detailed beginner Raylib tutorial & UI modification guide: [RAYLIB_UI_GUIDE.md](../../../docs/RAYLIB_UI_GUIDE.md).
* Geometric layout & test specifications: [stompbox_layout.md](./references/stompbox_layout.md).

### Mouse & Trackpad Interaction (`include/SimLayout.hpp`, `include/SimPot.hpp`)

* **Encoder Knob:**
  * **Short Click:** Next menu page (P1 $\to$ P12).
  * **Hold (1 Second):** Manual preset commit (`SAVED`).
  * **Mouse Wheel / Trackpad Scroll:** Turns the knob in discrete detents (`detents_from_wheel()`).
  * **Circular Drag:** Drag along outer rim clockwise to increment, counter-clockwise to decrement.
  * **Vertical Drag:** Click near knob center and drag straight up or down.
* **Expression Treadle:**
  * Drag or click along the vertical treadle slider on the right. Top = 100% (toe down), bottom = 0% (heel down).
* **Footswitches:**
  * Click **TAP** footswitch to tap tempo and reset LFO phase.
  * Click **MUTATE** footswitch to trigger immediate Turing shift register step.
* **Simulator Action Buttons:**
  * Click on-screen buttons: **Freeze**, **Bypass**, **Reset Defaults**, **Undo Reset**, **Slot -**, **Slot +**.

### Keyboard Shortcuts

| Key | Function | Key | Function |
| :--- | :--- | :--- | :--- |
| **`[E]`** / **`[Q]`** | Next / Previous Menu Page | **`[W]`** / **`[S]`** | Increase / Decrease Parameter Value |
| **`[T]`** / **`[Space]`** | Tap Tempo & Phase Reset | **`[M]`** | Mutate Turing Shift Register |
| **`[F]`** | Freeze held outputs | **`[B]`** | Bypass outputs (send 0s) |
| **`[R]`** | Reset all settings to defaults | **`[U]`** | Undo last reset |
| **`[`** / **`]`** | Select Preset Slot (1–8) | **`[Enter]`** (1s) | Hold to Save Preset |
| **`[1]` / `[2]` / `[3]`** | Set Expression (0%, 50%, 100%) | **`[ESC]`** | Exit Simulator |

---

## 🔌 CoreMIDI Integration (`include/MidiRouter.hpp`, `include/CoreMidiBackend.hpp`)

* **Target Matching:** By default searches for `"FRASIL FM-1"`. Matching is case- and punctuation-insensitive (`name_matches()`).
* **Auto-Reconnection:** If the hardware device is disconnected, the router marks it offline and scans every 1 second (`SCAN_INTERVAL_MS`). When reconnected, `midi_epoch` increments and all active CC values are resent immediately.
* **DAW Virtual Mode:** When `PHASE_BOX_MIDI_DEST=virtual`, creates a virtual CoreMIDI source named `"Phase Box Virtual Out"`.
* **Corrupted MIDI Subsystem Fix:** If macOS MIDI Studio freezes or fails to detect USB devices:
  ```bash
  bash scripts/reset_coremidi.sh --dry-run
  bash scripts/reset_coremidi.sh -y
  ```

---

## 🧪 Testing Simulator Layout & Potentiometers

Layout bounding boxes, detents, and drag vectors are fully unit-tested in GoogleTest:
```bash
./build-test/run_host_tests --gtest_filter="SimLayout.*:SimPot.*:KnobDragTest.*"
```
