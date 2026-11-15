# Phase Box: User Guide & Operating Manual

> **Implementation status (read first).** The desktop simulator and the RP2040 firmware now share the same engine, pages P1–P12, tap tempo, Turing machine and preset logic, and the same unit tests cover both. Current limitations:
> the firmware has been written, unit-tested on a computer, and successfully cross-compiled into target `.uf2` binaries (`firmware/build/phase_box_firmware.uf2`), but has **not yet been flashed to physical silicon** (check the OLED height and encoder direction upon first hardware flash, see the Developer Guide);
> presets on the device are saved to the RP2040's own flash (the simulator saves files); this is unit-tested but, like the rest of the firmware, not yet tried on a real board;
> the firmware does not follow external MIDI clock;
> the simulator sends MIDI only on macOS (CoreMIDI), straight to the destination named by `PHASE_BOX_MIDI_DEST` (default `FRASIL FM-1`); on Linux/Windows it runs without MIDI output.


Welcome to the **Phase Box** operating manual. Phase Box is a dedicated hardware modulation synthesizer powered by the Raspberry Pi RP2040 microcontroller. It translates complex algorithmic waveforms, generative shift registers, and polyrhythmic macro scaling into real-time MIDI Control Change (CC) data streams for video synthesizers (such as LZX Industries, Auto Waaaves, and r_e_c_u_r), audio synthesizers, and Digital Audio Workstations (DAWs).

---

## 1. Hardware Overview & Physical Interface

Phase Box features an intuitive physical control surface designed for live audiovisual performances and tactile studio experimentation.

```
       +-------------------------------------------------------+
       |   PHASE BOX                                   [USB-C] |
       |                                                       |
       |  +--------------------+                               |
       |  | P1: WAVE  [SINE] * |      ( O ) ROTARY ENCODER     |
       |  | ~~~~~~~~~~~~~~~~~~ |     (Turn: Value / Click: Page|
       |  | VAL: SINE          |      Hold 1s: Preset Save)    |
       |  +--------------------+                               |
       |    128x32/64 OLED                                     |
       |                                                       |
       |  [TAP / RESET]  ( O )   [MUTATE / CYCLE]   [BYPASS]   |
       |  Footswitch 1  EXPRESSION  Footswitch 2  Footswitch 3 |
       |               (click: freeze)                         |
       |                                                       |
       |  [MIDI OUT DIN/TRS]                                   |
       +-------------------------------------------------------+
```

### Physical Controls & Connectors

| Component | Hardware Pin | Purpose & Operation |
| :--- | :--- | :--- |
| **OLED Display** | GP4 (SDA), GP5 (SCL) | 128×32 / 128×64 graphical display showing active page, live waveform visualizer, and parameter values. |
| **Rotary Encoder** | GP2 (A), GP3 (B) | Turn to change the value on the current page (clockwise = up). On the BPM page one click is 1 BPM, and spinning the knob quickly steps 5 BPM per click. |
| **Encoder Pushbutton** | GP6 (Switch) | **Short Press:** Go to the next page (P1 … P12, then back to P1).<br/>**Long Press (1 Second):** Save the preset now (`SAVED`).<br/>**Press and turn:** Previous / next preset slot (1–8). The press then neither saves nor changes page. |
| **Footswitch 1 (Tap / Reset)** | GP10 | **Tap Tempo:** Tap 2 or more times to synchronize BPM (40–300 BPM).<br/>**Phase Reset:** Instantly restarts all LFO accumulators to $0^\circ$ on each tap. |
| **Footswitch 2 (Mutate)** | GP11 | Step/seed the probabilistic Turing Machine shift register to inject controlled algorithmic variation. |
| **Footswitches 1 + 2 together** | GP10 + GP11 | **Hold both for 2 seconds:** reset everything to defaults (`RESET`). Hold them again before changing anything else to undo it (`UNDONE`). See [Resetting everything to defaults](#resetting-everything-to-defaults). |
| **Footswitch 3 (Bypass)** | GP12 | Bypass on / off: every destination sends 0 (`BYPASS`). See [Freeze and bypass](#freeze-and-bypass). |
| **Expression Encoder** | GP7 (A), GP8 (B) | Turn to set the global modulation depth (0.00–1.00, 20 clicks end to end; clockwise = up). Scales every macro destination. |
| **Expression Encoder Pushbutton** | GP9 (Switch) | Freeze on / off (`FROZEN`). See [Freeze and bypass](#freeze-and-bypass). |
| **MIDI Output Port** | GP0 (TX), GP1 (RX) | Standard 31,250 baud serial MIDI (supports 5-pin DIN or Type-A/B 3.5mm TRS jack). |
| **USB-C Port** | Native RP2040 USB | Class-compliant USB MIDI device (no drivers required for macOS, Windows, Linux, or iOS). |

---

## 2. OLED Display & Menu Pages

The OLED interface is structured into pages. Click the encoder to move to the next page and turn it to change the value:

```
+------------------------------------+
| P1: WAVE D1                    S1* |  <-- Page Title, Dest & Slot/Dirty Tag
| ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ |  <-- Real-Time Destination Curve
| VAL: SINE                          |  <-- Current Parameter Readout / Status
+------------------------------------+
```

### Parameter Pages Reference

| Page | Parameter | Range / Values | Description |
| :--- | :--- | :--- | :--- |
| **`P1: WAVE`** | Waveform Shape | `SINE`, `SAW`, `SQR`, `TRI` | Selects the waveform every destination plays (shared by all destinations). |
| **`P2: BPM`** | Master Tempo | `40.0` – `300.0` BPM | Sets the base oscillator speed. Turn normally for 1 BPM steps; spin quickly for velocity-accelerated 5 BPM steps per detent. (Clicking encoder advances to next page). |
| **`P3: PHASE`** | Phase Offset | $0.00$ – $6.28$ ($2\pi$ rad) | Static phase rotation relative to the master clock, applied to every destination. Wraps around at $2\pi$. |
| **`P4: CHAN`** | Target MIDI Channel | `1` – `16` | MIDI Channel of the destination chosen on `P8` (each destination has its own). |
| **`P5: CC`** | Target MIDI CC | `0` – `127` | MIDI CC number of the destination chosen on `P8`. |
| **`P6: PM`** | Phase Modulation | `0.00` – `5.00` | Depth of internal phase modulation (waveform folding), applied to every destination. |
| **`P7: MUT`** | Turing Mutation | `0.00` – `1.00` ($0\% - 100\%$) | Probability that the Turing shift register inverts its feedback bit on each master cycle: `0.00` repeats a locked 16-step loop, around `0.50` is the most random, and `1.00` flips every time, which gives a fixed alternating pattern (not noise). Only audible when `P11` mix is above zero. |
| **`P8: MAC SEL`** | Active Macro Target | `Dest 1`, `Dest 2`, `Dest 3` | Selects which macro destination is currently active for configuration. |
| **`P9: MAC WGT`** | Macro Scale / Weight | `-2.00` to `+2.00` | Bipolar scaling factor of the destination chosen on `P8`. Negative values invert its waveform. |
| **`P10: MAC DIV`**| Polyrhythmic Multiplier | `0.125x` to `8.0x` | Clock multiplier of the destination chosen on `P8`, stepped through `0.125, 0.25, 0.5, 0.75, 1, 1.5, 2, 3, 4, 8`. One master cycle is one beat, so `0.5x` takes two beats per cycle and `4x` makes four cycles per beat. The OLED widens or narrows its window so you can see the ratio. |
| **`P11: TUR MIX`**| Turing Mix | `0.00` – `1.00` | How much of the Turing sequence replaces each destination's waveform (`0.00` = waveform only, `1.00` = Turing sequence only). The sequence steps once per master cycle. |
| **`P12: MAC OFF`**| Macro Offset / Floor | `-1.00` to `+1.00` | Baseline DC offset of the destination chosen on `P8`. Shifts the minimum and maximum CC output window up or down. |

---

## 3. Core Features & Operation

### 3.1 Polyrhythmic Macro Engine
Unlike conventional LFOs that send synchronized lock-step modulation, Phase Box provides a **Polyrhythmic Macro Engine** that streams **3 independent MIDI CC messages simultaneously**:
- Each destination outputs its own independent CC number and Channel.
- Each destination possesses its own **Clock Multiplier** (`MAC DIV`). For example:
  - **Destination 1:** Multiplier `1.0x` on CC 16, Channel 1 (Channels the main beat).
  - **Destination 2:** Multiplier `1.5x` (dotted feel) on CC 74, Channel 1 (Filter Cutoff).
  - **Destination 3:** Multiplier `0.5x` (a slow two-beat swell) on CC 10, Channel 2 (Pan / Color).
- When combined, the resulting modulation streams weave into complex polyrhythms that take several beats to line up again, so patterns and timbres keep evolving.

#### Multi-Destination Workflow (P8 Selector)
Pages **P4 (CHAN)**, **P5 (CC)**, **P9 (MAC WGT)**, **P10 (MAC DIV)**, and **P12 (MAC OFF)** modify whichever destination is selected on **P8 (MAC SEL)**:
1. Navigate to **`P8: MAC SEL`** and turn the knob to select `Dest 1`, `Dest 2`, or `Dest 3`.
2. Move to **`P4: CHAN`** to set that destination's MIDI Channel (1–16).
3. Move to **`P5: CC`** to assign its MIDI CC number (0–127).
4. Move to **`P9: MAC WGT`** to set its amplitude depth (`-2.00` to `+2.00`; negative values invert the wave).
5. Move to **`P10: MAC DIV`** to set its clock ratio (`0.125x` to `8.0x`).
6. Move to **`P12: MAC OFF`** to shift its baseline floor/ceiling (`-1.00` to `+1.00`) to position the CC window.
7. Select the next destination on `P8` and repeat. All 3 streams output concurrently every cycle and are automatically saved to the active preset slot.

### 3.2 Expression Encoder (Global Depth)
The second rotary encoder, between the footswitches, sets one depth value for all destinations:
- Each click moves the value by **0.05**; it stops at **0.00** and **1.00** (20 clicks end to end).
- At **0.00**: Modulation depth is halved (50%). This is the value at power-up.
- At **1.00**: Modulation is at full depth.
- It only scales depth. A destination with a **negative weight** is inverted at every expression value.
- The value is not stored in presets and `[R]` reset does not change it; it starts at 0.00 after every power-up.

### 3.3 Turing Machine Generative Mutation
The Turing machine takes one step per master cycle and its value is blended into every destination by `P11: TUR MIX` (default `0.00`, so nothing changes until you raise it). `P7: MUT` sets how much the sequence mutates; **Footswitch 2** (key `[M]` in the simulator) steps it immediately.
- At **$0\%$ mutation**: The shift register locks into an exact, repeating 16-step sequence.
- At **$5\% - 20\%$ mutation**: The sequence stays structured and musical, but individual steps drift over time.
- At **$50\%$ mutation**: The most random setting.
- At **$100\%$ mutation**: The feedback bit flips every time, so you get a fixed alternating pattern rather than noise.

### 3.4 Screen Burn-In Protection (Screensaver)
To protect OLED displays during long stage performances or static studio sessions:
- If no encoder or footswitch activity is detected for **30 seconds**, the screen contrast automatically dims to its minimum visibility setting (`0x01`).
- Turning or pressing either encoder, or stepping on any footswitch, wakes the display back to full brightness (`0xCF`) straight away.

### 3.5 Auto-Save & Preset Persistence
- **Automatic Background Save:** Whenever you tweak any saved parameter, an internal "dirty-state" timer begins. If no further adjustments are made for **2 seconds**, Phase Box silently writes your current configuration to the current preset slot. Anything that is not part of a preset (the page you are on, the expression value, the running phase) never triggers a save.
- **Manual Save:** Press and hold the encoder button for 1 second until the screen confirms `SAVED` (a bar fills along the divider while you hold). A shorter press is an ordinary short press and saves nothing.
- Settings automatically restore on power-up, silently.
- **Unsaved marker:** the top-right of the screen shows the slot, for example `S1`, and `S1*` while there are changes that have not been written yet.
- **Slots 1–8:** press and turn the encoder on the unit, or `[` and `]` in the simulator. Switching first saves any pending edits to the slot you are leaving, then loads the new one (`LOADED`). An empty slot (`EMPTY`) keeps your current settings and writes nothing until you change something or hold-save, so browsing empty slots never creates files.
- **If a save fails** the screen shows `SAVE FAILED`; auto-save then waits 10 seconds before retrying, and a manual hold-save always tries immediately. A damaged preset file shows `BAD PRESET` and is ignored.
- **On the unit:** presets live in the last 64 KB of the RP2040's flash and survive power-off and firmware updates that fit below that area. Each save pauses the unit for about a millisecond (about 45 ms every eighth save of a slot), so MIDI and the screen can hiccup very briefly. If power is lost during a save, the previous save of that slot is kept.
- **Simulator files:** presets are stored as `preset_1.bin` … `preset_8.bin` in the current directory, or in `$PHASE_BOX_PRESET_DIR` if set.

---

## 4. Desktop Native Simulator

If you do not have physical hardware in front of you, Phase Box includes a real-time desktop simulator that runs natively on macOS, Linux, and Windows.

```bash
# Build and launch the desktop simulator
cmake -B build-native -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build-native -j4
./build-native/phase_box_native_sim
```

### Keyboard Shortcuts in Simulator

| Key | Action |
| :--- | :--- |
| **`[E]`** | Next Page (Forward through P1–P12) |
| **`[Q]`** | Previous Page (Backward through P1–P12) |
| **`[W]` or `[Up Arrow]`** | Increase Current Parameter Value |
| **`[S]` or `[Down Arrow]`** | Decrease Current Parameter Value |
| **`[T]` or `[Space]`** | Tap Tempo |
| **`[M]`** | Step the Turing machine now (Mutate footswitch) |
| **`[Enter]`** (hold 1 s) | Save the current settings (`SAVED`) |
| **`[`** / **`]`** | Previous / next preset slot (1–8) |
| **`[R]`** | **Reset everything to defaults** (one keypress; shows `RESET`) |
| **`[U]`** | Undo the last reset (shows `UNDONE`) |
| **`[F]`** | **Freeze** / resume: hold every output where it is (shows `FROZEN`) |
| **`[B]`** | **Bypass** on/off: every destination sends 0 (shows `BYPASS`) |
| **`[Z]`** / **`[X]`** | Expression encoder one click down / up |
| **`[ESC]`** | Exit Simulator |

### Mouse & Interactive Stompbox Controls
The simulator UI displays a scaled physical stompbox enclosure with fully interactive mouse and trackpad controls:
* **Rotary Knob:**
  * **Click:** Advance to the next menu page (equivalent to pressing `ENC_SW`).
  * **Hold (1 Second):** Fills the hold-to-save ring around the knob and commits preset save (`SAVED`).
  * **Scroll Wheel / Trackpad:** Scrolling over the knob steps parameter values in whole detents.
  * **Circular Drag:** Click on the knob circumference and drag circularly around the knob center (clockwise = value up, counter-clockwise = value down).
  * **Vertical Drag:** Click near the center of the knob and drag straight up/down to adjust values.
* **Expression Encoder** (between TAP and MUTATE):
  * Drag around it (or straight up/down from its centre), or scroll over it, to turn it in whole clicks. The amber ring and the number under it show the value (0.00–1.00).
  * Click it (without dragging) to freeze / unfreeze, like pressing it on the unit.
* **Footswitches:**
  * Click the **TAP** footswitch to tap tempo and reset master LFO phase.
  * Click the **MUTATE** footswitch to step the Turing shift register immediately.
  * Click the **BYPASS** footswitch to switch bypass on / off; the LED above it is lit while bypassed.
* **On-Screen Shortcut Buttons:**
  * Click the buttons alongside the enclosure (**Freeze**, **Bypass**, **Reset Defaults**, **Undo Reset**, **Slot -**, **Slot +**) for one-click action triggering. The simulator has no press-and-turn or two-footswitch hold (one mouse), so reset, undo and slots are buttons and keys here.

### Freeze and bypass
- **Freeze** (expression encoder click on the unit, `[F]` in the simulator): stops the LFOs and holds every destination at the value it has right now, even if you turn the expression encoder or edit settings. Press it again and it carries on from the same point (no jump). The Turing machine pauses too. The screen shows `FROZEN`.
- **Bypass** (Footswitch 3 on the unit, `[B]` in the simulator): every destination sends **0** and the display shows a flat line (`BYPASS`). The LFOs keep running silently, so when you switch back they are still in time. Bypass wins over freeze; if you were frozen, letting go of bypass returns to the held values.
- Bypass sends exactly 0 on each destination's CC, which is not always a "neutral" value for the synth parameter (for example, 0 on a filter cutoff is fully closed).
- Neither is saved in presets, and reset does not change them.

### Resetting everything to defaults
On the unit, **hold Footswitches 1 and 2 together for 2 seconds** (press them at the same time or one after the other). In the simulator, press **`[R]`**. Every setting goes back to how the unit starts up:
- **Reset:** waveform, BPM (120), phase offset, phase modulation, Turing mutation and mix, the Turing pattern, and every destination's channel, CC, weight, offset and multiplier. All LFOs restart at 0 degrees.
- **Not reset:** the expression encoder value (`[Z]`/`[X]`; a performance control, not a setting), the page you are on, which destination is selected, and which preset slot you are in.
- **It is saved like any other edit.** About 2 seconds later auto-save writes the defaults into the current slot, which replaces what was saved there.
- **Undo: hold Footswitches 1 + 2 for 2 seconds again** (`[U]` in the simulator). It brings back exactly what you had before the reset, even if auto-save has already stored the defaults. Undo works for one step and only until you change another setting (then it shows `EDITED SINCE RESET`, and your new change is kept). Switching preset slots also cancels it, and quitting forgets it.
- Resetting when everything is already at defaults shows `ALREADY DEFAULT` and changes nothing.
- On the unit, holding the footswitches undoes when there is something to undo, and resets otherwise. Pressing them also taps Footswitch 1 (the LFOs restart at 0 degrees; the tempo is not changed) and steps the Turing machine once.

### Connecting Simulator to Hardware & DAWs
By default the simulator sends MIDI **straight to a hardware destination named `FRASIL FM-1`**. It creates no virtual port, and nothing needs patching in Audio MIDI Setup. The bottom line of the window shows where it is going, for example `MIDI out: -> FRASIL FM-1`.
- **Find the device by name:** set `PHASE_BOX_MIDI_DEST` to any part of the name, e.g. `PHASE_BOX_MIDI_DEST="Other Synth" ./build-native/phase_box_native_sim`. Capitals, spaces and dashes don't matter (`frasil fm 1` matches). If nothing matches, **nothing is sent** (it never falls back to another device) and the window lists the destinations it did see.
- **Plug in later or re-plug:** it looks again every second, notices when the device is unplugged, and reconnects (resending every value) when it is back.
- **What does CoreMIDI see?** `./build-native/phase_box_native_sim --list-midi` prints the destinations and sources and exits. It works even when the Audio MIDI Setup window is blank.
- **To feed a DAW instead** (Ableton / Logic / Reaper), start it with `PHASE_BOX_MIDI_DEST=virtual`. That creates the virtual source `Phase Box Virtual Out`, which you enable in the DAW's MIDI preferences; routing it to hardware needs a patchbay such as [MIDI Patchbay](https://notahat.com/midi_patchbay/).
- **To control the FRASIL FM-1's parameters:** each destination has its own channel and CC. On `P8` pick a destination, then set `P4` (channel) and `P5` (CC). Changes are saved automatically.
- **If macOS MIDI is stuck** (blank MIDI Studio, red outlines, missing devices): `bash scripts/reset_coremidi.sh --dry-run` shows what it would do, then run it without `--dry-run`. It moves the Audio MIDI Setup preferences and saved layouts into a backup folder (nothing is deleted) and restarts the MIDI services.