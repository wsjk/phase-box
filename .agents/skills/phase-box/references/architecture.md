# Phase Box Architecture Reference

## Overview
Phase Box is an RP2040-powered algorithmic modulation engine generating MIDI CC streams for video synthesizers (LZX, Auto Waaaves, r_e_c_u_r).

## Dual-Core Workload Split
- **Core 0 (Real-Time 1000 Hz Interrupt)**:
  - Phase accumulators & waveform generation (`PhaseLFO`)
  - Footswitch tap tempo & 24 PPQN MIDI clock tracking (`ClockManager`)
  - MIDI CC message generation (`MidiRouter`)
  - Non-blocking UART output at 31250 baud (`PicoHalMidi`)
- **Core 1 (UI & Storage)**:
  - SSD1306 128x32/128x64 OLED display rendering (throttled to 30 FPS)
  - Rotary encoder menu navigation & state machine
  - Flash preset storage via LittleFS (`HalFlash`)

## Hardware Interface
- **MIDI UART**: `uart0`, TX on GP0, RX on GP1, 31250 baud (8N1).
- **Footswitches**:
  - `FootswitchTapMode`: Tap tempo delta tracking & instant LFO phase reset.
  - `FootswitchMutation`: Turing shift register mutation trigger / cycle.
- **Rotary Encoder**: Incremental encoder with integrated push button (`EncoderSwitch` / 1s hold-to-save preset).
- **OLED Display**: SSD1306 over I2C at 1 MHz (Fast-Mode Plus).

## Core Modules
- `include/core/phase_lfo.hpp`: 32-bit phase accumulator, 256-entry Sine LUT, Triangle, Square, S&H, Turing Mutation.
- `include/core/clock_manager.hpp`: Tap tempo (20–300 BPM), MIDI 24 PPQN parser, 1s internal fallback timeout.
- `include/core/midi_router.hpp`: 4-channel CC routing (channels 1–16, CC 0–127).
- `include/core/modulation_engine.hpp`: Top-level aggregation of 4 LFOs, clock, and router.
- `include/core/core0_task.hpp`: 1000 Hz real-time interrupt loop and MIDI dispatcher.

## HAL Layer
- `include/hal/hal_midi.hpp` & `pico_hal_midi.hpp`: Hardware UART & `mock_hal_midi.hpp` for simulation.
- `include/hal/hal_gpio.hpp` & `mock_hal_gpio.hpp`: Tactile button & encoder abstraction.
- `include/hal/hal_flash.hpp`: Preset serialization & SPI NOR flash abstraction.
