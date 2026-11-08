# Phase Box Architecture Reference

## Overview
Phase Box is an RP2040-powered polyrhythmic modulation engine and MIDI synthesizer generating Control Change (CC) streams for video synthesizers (LZX Industries, Auto Waaaves, r_e_c_u_r), audio synths, and DAWs over both 31,250 baud serial MIDI and USB-MIDI.

## Production Stack vs. Legacy Experiments
* **Production Stack (Active):** `include/DeviceApp.hpp`, `include/PhaseBoxState.hpp`, `include/MacroEngine.hpp`, `include/SimControls.hpp`, `include/PresetSession.hpp`, `include/OledRenderer.hpp`, `include/ScreenSaver.hpp`, `include/hal/pinout.hpp`.
* **Legacy Prototype (Unbuilt):** `include/core/` and `src/main.cpp`. Do not edit or import these.

## Dual-Core Workload Split
- **Core 0 (Real-Time Control & MIDI Engine)**:
  - Input polling: Quadrature encoder (GP2/GP3), debounced switches (GP6 encoder push, GP10 tap tempo, GP11 mutate), smoothed ADC0 expression pedal (GP26).
  - TinyUSB device servicing (`tud_task()`).
  - 60 Hz modulation frame step (`PhaseBoxState::advance()` & `evaluateMacros()`).
  - Polyrhythmic macro clock multipliers (0.125x to 8x across 3 destinations).
  - 16-bit Turing LFSR mutation on master cycle wrap or footswitch tap.
  - Bandwidth limiter (`MidiChangeFilter`): sends only on CC change + 1s periodic refresh.
  - Dual MIDI output: UART0 TX at 31,250 baud (GP0) and TinyUSB USB-MIDI.
  - OLED rasterization at ~30 FPS into `SeqlockBuffer`.
- **Core 1 (Async Display & Screensaver)**:
  - SSD1306 128x32/128x64 OLED display controller on `i2c0` (GP4 SDA, GP5 SCL) at 400 kHz / 1 MHz.
  - Reads finished frames from `SeqlockBuffer` without stalling Core 0.
  - `ScreenSaver` contrast dimming (dims to 0x01 after 30s of inactivity, restores to 0xCF on activity).

## Hardware Interface
- **MIDI UART**: `uart0`, TX on GP0, RX on GP1, 31,250 baud (8N1).
- **Expression Pedal (ADC)**: GP26 (ADC0), normalized 0.0f - 1.0f with EMA filter.
- **Footswitches**:
  - `FS_TAP` (GP10): Tap tempo (40–300 BPM) & instant LFO phase reset.
  - `FS_MUTATE` (GP11): Turing shift register mutation trigger / cycle.
- **Rotary Encoder**: Incremental encoder on GP2/GP3 with integrated push button on GP6 (`ENC_SW`).
  - Turn: parameter adjust (velocity-accelerated: 1 BPM normal, 5 BPM fast spin).
  - Click: advance page P1..P11.
  - Hold 1s: manual preset save (`SAVED`).
- **OLED Display**: SSD1306 over I2C0 (GP4 SDA, GP5 SCL) at 400 kHz / 1 MHz.

## Core Modules & Organization
- `include/DeviceApp.hpp`: Main hardware-injected coordinator (encoder, switches, pedal, MIDI, OLED buffer).
- `include/PhaseBoxState.hpp`: Top-level state bundle, freeze/bypass modes, oscillator, Turing, macros.
- `include/SimControls.hpp`: Menu pages P1–P11 adjust logic, footers, live curve generation.
- `include/MacroEngine.hpp`: Polyrhythmic clock division (0.125x to 8x) and bipolar scaling.
- `include/PhaseOscillator.hpp`: Sine LUT, Saw, Square, Triangle, and phase modulation.
- `include/TuringMachine.hpp`: 16-bit shift register with probabilistic mutation.
- `include/PresetSession.hpp`: Auto-save dirty state timer (2s idle), hold-to-save (1s), slots 1–8.
- `include/OledRenderer.hpp`: Dual-panel (128x32 and 128x64) graphical display layout.
- `include/ScreenSaver.hpp`: Contrast register dimming for burn-in protection.
- `include/MidiChangeFilter.hpp`: Bandwidth-compliant CC transmission filter.
- `include/hal/pinout.hpp`: Single source of truth for board wiring and encoder transitions.
- `include/hal/seqlock_buffer.hpp`: Lock-free inter-core frame transfer.
