# Phase Box: User Guide & Operating Manual

> **Implementation status (read first).** This guide describes the intended design. Current limitations:
> the OLED pages, encoder and footswitches exist only in the desktop simulator (pages P1–P11 all change the MIDI it sends; the firmware loop shares the same engine but has no menu yet);
> auto-save, hold-to-save, the `SAVED` confirmation and restore-on-start all work in the simulator (settings are files); the RP2040 firmware has no flash-backed storage plugged in yet, so nothing is kept across power cycles on the device;
> the firmware build does not yet wire up the OLED menu, encoder, footswitches or expression-pedal ADC.
> The simulator sends MIDI only on macOS (CoreMIDI); on Linux/Windows it runs without MIDI output.


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
       |   [ TAP / RESET ]               [ MUTATE / CYCLE ]    |
       |    Footswitch 1                    Footswitch 2       |
       |                                                       |
       |  [MIDI OUT DIN/TRS]               [EXPRESSION IN]     |
       +-------------------------------------------------------+
```

### Physical Controls & Connectors

| Component | Hardware Pin | Purpose & Operation |
| :--- | :--- | :--- |
| **OLED Display** | GP4 (SDA), GP5 (SCL) | 128×32 / 128×64 graphical display showing active page, live waveform visualizer, and parameter values. |
| **Rotary Encoder** | GP2 (A), GP3 (B) | Turn clockwise/counter-clockwise to navigate pages and adjust numeric parameters. Features dynamic velocity acceleration (turning faster changes values in larger increments). |
| **Encoder Pushbutton** | GP6 (Switch) | **Short Press:** Toggle sub-modes (e.g. Coarse vs. Fine BPM tuning).<br/>**Long Press (1 Second):** Trigger non-volatile preset save. |
| **Footswitch 1 (Tap / Reset)** | GP10 | **Tap Tempo:** Tap 2 or more times to synchronize BPM.<br/>**Phase Reset:** Instantly restarts all LFO accumulators to $0^\circ$ on each tap. |
| **Footswitch 2 (Mutate)** | GP11 | Step/seed the probabilistic Turing Machine shift register to inject controlled algorithmic variation. |
| **Expression Pedal Jack** | GP26 (ADC0) | Accepts standard TRS expression pedals (0–3.3V analog voltage) for bipolar scaling of all macro destinations. |
| **MIDI Output Port** | GP0 (TX), GP1 (RX) | Standard 31,250 baud serial MIDI (supports 5-pin DIN or Type-A/B 3.5mm TRS jack). |
| **USB-C Port** | Native RP2040 USB | Class-compliant USB MIDI device (no drivers required for macOS, Windows, Linux, or iOS). |

---

## 2. OLED Display & Menu Pages

The OLED interface is structured into pages. Turn the encoder to navigate or adjust settings:

```
+------------------------------------+
| P1: WAVE                       [*] |  <-- Page Title & Edit Indicator (*)
| ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ |  <-- Real-Time Waveform Oscilloscope
| VAL: SINE                          |  <-- Current Parameter Readout
+------------------------------------+
```

### Parameter Pages Reference

| Page | Parameter | Range / Values | Description |
| :--- | :--- | :--- | :--- |
| **`P1: WAVE`** | Waveform Shape | `SINE`, `SAW`, `SQR`, `TRI` | Selects the waveform every destination plays (shared by all destinations). |
| **`P2: BPM`** | Master Tempo | `20.0` – `300.0` BPM | Sets the base oscillator speed. Click encoder to toggle between **Fine** (0.1 BPM) and **Coarse** (1.0–5.0 BPM) adjustment. |
| **`P3: PHASE`** | Phase Offset | $0.00$ – $6.28$ ($2\pi$ rad) | Static phase rotation relative to the master clock, applied to every destination. Wraps around at $2\pi$. |
| **`P4: CHAN`** | Target MIDI Channel | `1` – `16` | MIDI Channel of the destination chosen on `P8` (each destination has its own). |
| **`P5: CC`** | Target MIDI CC | `0` – `127` | MIDI CC number of the destination chosen on `P8`. |
| **`P6: PM`** | Phase Modulation | `0.00` – `5.00` | Depth of internal phase modulation (waveform folding), applied to every destination. |
| **`P7: MUT`** | Turing Mutation | `0.00` – `1.00` ($0\% - 100\%$) | Probability that the Turing shift register inverts its feedback bit on each master cycle: `0.00` repeats a locked 16-step loop, around `0.50` is the most random, and `1.00` flips every time, which gives a fixed alternating pattern (not noise). Only audible when `P11` mix is above zero. |
| **`P8: MAC SEL`** | Active Macro Target | `Dest 1`, `Dest 2`, `Dest 3` | Selects which macro destination is currently active for configuration. |
| **`P9: MAC WGT`** | Macro Scale / Weight | `-2.00` to `+2.00` | Bipolar scaling factor of the destination chosen on `P8`. Negative values invert its waveform. |
| **`P10: MAC DIV`**| Polyrhythmic Multiplier | `0.125x` to `8.0x` | Clock multiplier of the destination chosen on `P8`, stepped through `0.125, 0.25, 0.5, 0.75, 1, 1.5, 2, 3, 4, 8`. One master cycle is one beat, so `0.5x` takes two beats per cycle and `4x` makes four cycles per beat. The OLED widens or narrows its window so you can see the ratio. |
| **`P11: TUR MIX`**| Turing Mix | `0.00` – `1.00` | How much of the Turing sequence replaces each destination's waveform (`0.00` = waveform only, `1.00` = Turing sequence only). The sequence steps once per master cycle. |

---

## 3. Core Features & Operation

### 3.1 Polyrhythmic Macro Engine
Unlike conventional LFOs that send synchronized lock-step modulation, Phase Box provides a **Polyrhythmic Macro Engine**:
- Each destination outputs its own independent CC number and Channel.
- Each destination possesses its own **Clock Multiplier** (`MAC DIV`). For example:
  - **Destination 1:** Multiplier `1.0x` on CC 16 (Channels the main beat).
  - **Destination 2:** Multiplier `1.5x` (dotted feel) on CC 74 (Filter Cutoff).
  - **Destination 3:** Multiplier `0.5x` (a slow two-beat swell) on CC 10, Channel 2 (Pan / Color).
- When combined, the resulting modulation streams weave into complex polyrhythms that take several beats to line up again, so patterns and timbres keep evolving.

### 3.2 Bipolar Expression Pedal Control
Plugging a standard expression pedal into the 1/4" input allows hands-free master morphing:
- At **heel down (0.0)**: Modulation depth is halved (50%).
- At **toe down (1.0)**: Modulation is at full depth.
- The pedal only scales depth. A destination with a **negative weight** is inverted at every pedal position, not just when the pedal moves.

### 3.3 Turing Machine Generative Mutation
The Turing machine takes one step per master cycle and its value is blended into every destination by `P11: TUR MIX` (default `0.00`, so nothing changes until you raise it). `P7: MUT` sets how much the sequence mutates; **Footswitch 2** (key `[M]` in the simulator) steps it immediately.
- At **$0\%$ mutation**: The shift register locks into an exact, repeating 16-step sequence.
- At **$5\% - 20\%$ mutation**: The sequence stays structured and musical, but individual steps drift over time.
- At **$50\%$ mutation**: The most random setting.
- At **$100\%$ mutation**: The feedback bit flips every time, so you get a fixed alternating pattern rather than noise.

### 3.4 Screen Burn-In Protection (Screensaver)
To protect OLED displays during long stage performances or static studio sessions:
- If no encoder or footswitch activity is detected for **30 seconds**, the screen contrast automatically dims to its minimum visibility setting (`0x01`).
- Touching or turning any control **instantly wakes** the display back to full operational brightness (`0xCF`) with zero latency.

### 3.5 Auto-Save & Preset Persistence
- **Automatic Background Save:** Whenever you tweak any saved parameter, an internal "dirty-state" timer begins. If no further adjustments are made for **2 seconds**, Phase Box silently writes your current configuration to the current preset slot. Anything that is not part of a preset (the page you are on, the pedal position, the running phase) never triggers a save.
- **Manual Save:** Press and hold the encoder button for 1 second until the screen confirms `SAVED` (a bar fills along the divider while you hold). A shorter press is an ordinary short press and saves nothing.
- Settings automatically restore on power-up, silently.
- **Unsaved marker:** the top-right of the screen shows the slot, for example `S1`, and `S1*` while there are changes that have not been written yet.
- **Slots 1–8:** `[` and `]` in the simulator. Switching first saves any pending edits to the slot you are leaving, then loads the new one (`LOADED`). An empty slot (`EMPTY`) keeps your current settings and writes nothing until you change something or hold-save, so browsing empty slots never creates files.
- **If a save fails** the screen shows `SAVE FAILED`; auto-save then waits 10 seconds before retrying, and a manual hold-save always tries immediately. A damaged preset file shows `BAD PRESET` and is ignored.
- **Simulator files:** presets are stored as `preset_1.bin` … `preset_8.bin` in the current directory, or in `$PHASE_BOX_PRESET_DIR` if set.

---

## 4. Desktop Native Simulator

If you do not have physical hardware in front of you, Phase Box includes a real-time desktop simulator that runs natively on macOS, Linux, and Windows.

```bash
# Launch the desktop simulator
./build-native/phase_box_native_sim
```

### Keyboard Shortcuts in Simulator

| Key | Action |
| :--- | :--- |
| **`[E]`** | Next Page (Forward through P1–P11) |
| **`[Q]`** | Previous Page (Backward through P1–P11) |
| **`[W]` or `[Up Arrow]`** | Increase Current Parameter Value |
| **`[S]` or `[Down Arrow]`** | Decrease Current Parameter Value |
| **`[T]` or `[Space]`** | Tap Tempo |
| **`[M]`** | Step the Turing machine now (Mutate footswitch) |
| **`[Enter]`** (hold 1 s) | Save the current settings (`SAVED`) |
| **`[`** / **`]`** | Previous / next preset slot (1–8) |
| **`[1]`** | Simulate Expression Pedal at 0% (Heel down) |
| **`[2]`** | Simulate Expression Pedal at 50% (Midway) |
| **`[3]`** | Simulate Expression Pedal at 100% (Toe down) |
| **`[ESC]`** | Exit Simulator |

### Connecting Simulator to Hardware & DAWs
The simulator creates a virtual MIDI device called **`Phase Box Virtual Out`**:
- **In Ableton / Logic / Reaper:** Open *Preferences $\rightarrow$ MIDI*, find `Phase Box Virtual Out`, and enable *Track / Remote*. Use standard MIDI Learn to map knobs.
- **To External Hardware Synths:** Route `Phase Box Virtual Out` to your USB-to-MIDI interface using free utilities like [Snoize MIDI Monitor](https://www.snoize.com/midimonitor/) or [MIDI Patchbay](https://notahat.com/midi_patchbay/).
