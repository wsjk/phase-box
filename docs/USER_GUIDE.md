# Phase Box: User Guide & Operating Manual

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
| **`P1: WAVE`** | Waveform Shape | `SINE`, `SAW`, `SQR`, `TRI` | Selects base oscillator waveform geometry. |
| **`P2: BPM`** | Master Tempo | `20.0` – `300.0` BPM | Sets the base oscillator speed. Click encoder to toggle between **Fine** (0.1 BPM) and **Coarse** (1.0–5.0 BPM) adjustment. |
| **`P3: PHASE`** | Phase Offset | $0.00$ – $6.28$ ($2\pi$ rad) | Static phase rotation relative to the master clock. |
| **`P4: CHAN`** | Target MIDI Channel | `1` – `16` | MIDI Channel assigned to the active destination. |
| **`P5: CC`** | Target MIDI CC | `0` – `127` | MIDI CC number assigned to the active destination. |
| **`P6: PM`** | Phase Modulation | `0.00` – `5.00` | Depth of secondary internal phase modulation (through-zero FM/PM style waveform folding). |
| **`P7: MUT`** | Turing Mutation | `0.00` – `1.00` ($0\% - 100\%$) | Probability that shift register feedback bits invert on each clock cycle, transitioning from locked loops to pure entropy. |
| **`P8: MAC SEL`** | Active Macro Target | `Dest 1`, `Dest 2`, `Dest 3` | Selects which macro destination is currently active for configuration. |
| **`P9: MAC WGT`** | Macro Scale / Weight | `-2.00` to `+2.00` | Bipolar scaling factor applied to the active macro output. Negative values invert the modulation envelope. |
| **`P10: MAC DIV`**| Polyrhythmic Multiplier | `0.125x` to `8.0x` | Independent clock multiplier/subdivision ratio for the active destination ($1/4\times$, $1/2\times$, $1\times$, $1.5\times$, $2\times$, etc.). |

---

## 3. Core Features & Operation

### 3.1 Polyrhythmic Macro Engine
Unlike conventional LFOs that send synchronized lock-step modulation, Phase Box provides a **Polyrhythmic Macro Engine**:
- Each destination outputs its own independent CC number and Channel.
- Each destination possesses its own **Clock Multiplier** (`MAC DIV`). For example:
  - **Destination 1:** Multiplier `1.0x` on CC 16 (Channels the main beat).
  - **Destination 2:** Multiplier `1.5x` (dotted feel) on CC 74 (Filter Cutoff).
  - **Destination 3:** Multiplier `0.25x` (long 4-bar swell) on CC 10 (Pan / Color).
- When combined, the resulting modulation streams weave into complex polyrhythms that keep video feedback patterns and synthesizer timbres continuously evolving without repetitive cycling.

### 3.2 Bipolar Expression Pedal Control
Plugging a standard expression pedal into the 1/4" input allows hands-free master morphing:
- At **heel down (0.0)**: Modulation depths attenuate to baseline offsets.
- At **toe down (1.0)**: Modulation expands to maximum peak-to-peak amplitude.
- If a destination has a **negative weight** (e.g. `-1.0`), pushing the expression pedal down *inverts* that specific channel's output while channels with positive weights scale upwards.

### 3.3 Turing Machine Generative Mutation
By navigating to `P7: MUT` or tapping **Footswitch 2 (Mutate)**:
- At **$0\%$ mutation**: The Turing shift register locks into an exact, repeating 16-step rhythmic sequence.
- At **$5\% - 20\%$ mutation**: The sequence remains structured and musical, but subtly mutates individual steps over time.
- At **$100\%$ mutation**: The sequence becomes completely chaotic and unpredictable white noise / random sample-and-hold.

### 3.4 Screen Burn-In Protection (Screensaver)
To protect OLED displays during long stage performances or static studio sessions:
- If no encoder or footswitch activity is detected for **30 seconds**, the screen contrast automatically dims to its minimum visibility setting (`0x01`).
- Touching or turning any control **instantly wakes** the display back to full operational brightness (`0xCF`) with zero latency.

### 3.5 Auto-Save & Preset Persistence
- **Automatic Background Save:** Whenever you tweak any parameter, an internal "dirty-state" timer begins. If no further adjustments are made for **2 seconds**, Phase Box silently writes your current configuration to flash storage.
- **Manual Save:** Press and hold the encoder button for 1 second until the screen confirms `SAVED`.
- Settings automatically restore on power-up.

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
| **`[E]`** | Next Page (Forward through P1–P9) |
| **`[Q]`** | Previous Page (Backward through P1–P9) |
| **`[W]` or `[Up Arrow]`** | Increase Current Parameter Value |
| **`[S]` or `[Down Arrow]`** | Decrease Current Parameter Value |
| **`[T]` or `[Space]`** | Tap Tempo & Phase Reset |
| **`[1]`** | Simulate Expression Pedal at 0% (Heel down) |
| **`[2]`** | Simulate Expression Pedal at 50% (Midway) |
| **`[3]`** | Simulate Expression Pedal at 100% (Toe down) |
| **`[ESC]`** | Exit Simulator |

### Connecting Simulator to Hardware & DAWs
The simulator creates a virtual MIDI device called **`Phase Box Virtual Out`**:
- **In Ableton / Logic / Reaper:** Open *Preferences $\rightarrow$ MIDI*, find `Phase Box Virtual Out`, and enable *Track / Remote*. Use standard MIDI Learn to map knobs.
- **To External Hardware Synths:** Route `Phase Box Virtual Out` to your USB-to-MIDI interface using free utilities like [Snoize MIDI Monitor](https://www.snoize.com/midimonitor/) or [MIDI Patchbay](https://notahat.com/midi_patchbay/).
