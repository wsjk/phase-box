---
name: phase-box-dsp-macro
description: >-
  Modulation engine, waveform synthesis, Turing machine LFSR, and menu parameter runbook for Phase Box.
  Use when adding new waveforms, modifying polyrhythmic clock division math,
  extending menu pages P1–P12 (SimControls), or adjusting the 60 Hz DSP signal flow.
---

# Phase Box DSP & Modulation Engine Skill

This skill provides procedures for maintaining and extending the core synthesis math, polyrhythmic macro scaling, Turing machine LFSR, and menu control logic.

---

## 🌊 60 Hz Modulation Pipeline

Every 16,667 µs (`FRAME_US`), the engine steps the modulation state machine:

```
[ Master Phase Accumulator (BPM 40-300 / Tap Tempo) ]
                     |
         +-----------+-----------+
         |                       |
[ Master Wrap Check ]            v
         |            [ Per-Destination Clock Multiplier ]
[ Turing LFSR Step ]  (0.125x to 8.0x per Macro Dest 1..3)
         |                       |
         +---------> [ Waveform Generator (with Phase Mod) ]
                           |
                     [ Bipolar Scale & DC Offset ]
                           |
                     [ Expression Pedal Scaling ]
                           |
                     [ Performance Filter (Freeze / Bypass) ]
                           |
                     [ Clamping [0, 127] ]
                           |
                     [ MidiChangeFilter (Rate Limiter) ]
                           |
                     [ UART0 GP0 + TinyUSB MIDI ]
```

See [pipeline_and_formulas.md](./references/pipeline_and_formulas.md) for exact KaTeX mathematical definitions.

---

## 🛠️ Step-by-Step Recipes

### Recipe 1: Adding a New Waveform Shape

1. **Add enum value** in `include/PhaseOscillator.hpp`:
   ```cpp
   enum WaveShape { SINE, SAW, SQUARE, TRIANGLE, MY_NEW_WAVE, COUNT };
   ```
2. **Implement generator math** in `PhaseOscillator::update()` (`src/PhaseOscillator.cpp`):
   ```cpp
   case MY_NEW_WAVE: {
       // phase is in [0.0f, 2*PI)
       // Compute normalized output in [0.0f, 1.0f]
       value = 0.5f + 0.5f * std::sin(phase + pm_depth * std::cos(phase));
       break;
   }
   ```
3. **Register display name** in `include/SimControls.hpp`:
   ```cpp
   inline const char* wave_name(int shape) {
       switch (shape) {
           case 0: return "SINE";
           case 1: return "SAW";
           case 2: return "SQR";
           case 3: return "TRI";
           case 4: return "MYWV";
       }
       return "???";
   }
   ```
4. **Update tests** in `tests/test_sim_controls.cpp` and verify:
   ```bash
   ./build-test/run_host_tests --gtest_filter="*Oscillator*"
   ```

---

### Recipe 2: Adding or Modifying a Menu Page (P1–P12)

Menu pages are defined in `include/SimControls.hpp` under namespace `simui`:

1. **Page Index Table:**
   - `P1`: Waveform Shape (`WAVE`)
   - `P2`: Master Tempo (`BPM`)
   - `P3`: Static Phase Offset (`PHASE`)
   - `P4`: Active Destination MIDI Channel (`CHAN`)
   - `P5`: Active Destination MIDI CC Number (`CC`)
   - `P6`: Phase Modulation Depth (`PM`)
   - `P7`: Turing Mutation Probability (`MUT`)
   - `P8`: Active Macro Destination Selector (`MAC SEL`: Dest 1, 2, or 3)
   - `P9`: Active Destination Amplitude Weight (`MAC WGT`: -2.00 to +2.00)
   - `P10`: Active Destination Clock Multiplier (`MAC DIV`: 0.125x to 8x)
   - `P11`: Turing LFSR Blending Mix (`TUR MIX`: 0.00 to 1.00)
   - `P12`: Active Destination Baseline Offset (`MAC OFF`: -1.00 to +1.00)

2. **Editing Handlers in `include/SimControls.hpp`:**
   - `simui::page_name(int page, const PhaseBoxState& s)`: Title bar text.
   - `simui::footer(int page, const PhaseBoxState& s)`: Status readout text.
   - `simui::adjust(PhaseBoxState& s, int page, int detents, bool fast)`: Value alteration logic.
   - `simui::preview_curve(const PhaseBoxState& s, int x, int width)`: Real-time OLED destination curve rasterizer.

3. **Verify Potentiometer Limits:**
   When adding or changing pages, update `include/SimPot.hpp` and verify `tests/test_sim_pot.cpp`:
   ```bash
   ./build-test/run_host_tests --gtest_filter="SimPot.*"
   ```

---

## 🎛️ Turing LFSR Generator (`include/TuringMachine.hpp`)

* 16-bit shift register stepped once per master cycle wrap or manual footswitch tap.
* `mutation_prob` ($0.0 - 1.0$): Chance of inverting the recirculated bit.
  - $0.0$: Locked 16-step repeating loop.
  - $0.05 - 0.20$: Evolving musical variation.
  - $0.50$: Maximum pseudorandom entropy.
  - $1.00$: Inverts bit every cycle (deterministic alternating cycle).
* Normalization: 8-bit output normalized to $[0.0, 1.0]$.
