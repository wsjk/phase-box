# Phase Box Architecture Reference

## Overview
Phase Box is an RP2040-powered algorithmic modulation engine generating MIDI CC streams for video synthesizers (LZX, Auto Waaaves, r_e_c_u_r).

## Dual-Core Workload Split
- **Core 0 (Real-Time 1000 Hz Interrupt)**:
  - Phase accumulators & waveform generation (`PhaseLFO`)
  - Nested cross-modulation between LFO pairs (`set_cross_modulation`)
  - Multi-destination macro scaling from expression pedal (`MacroEngine`)
  - Footswitch tap tempo & 24 PPQN MIDI clock tracking (`ClockManager`)
  - MIDI CC message generation (`MidiRouter`)
  - Non-blocking UART output at 31250 baud (`PicoHalMidi`)
  - ADC0 sampling on GP26 (`PicoHalAdc`)
- **Core 1 (UI & Storage)**:
  - SSD1306 128x32/128x64 OLED display rendering (throttled to 30 FPS)
  - Rotary encoder menu navigation & state machine
  - Flash preset storage via LittleFS / raw flash sector erase (`PicoHalFlash`)

## Hardware Interface
- **MIDI UART**: `uart0`, TX on GP0, RX on GP1, 31250 baud (8N1).
- **Expression Pedal (ADC)**: GP26 (ADC0), normalized 0.0f - 1.0f.
- **Footswitches**:
  - `FootswitchTapMode` (GP10): Tap tempo delta tracking & instant LFO phase reset.
  - `FootswitchMutation` (GP11): Turing shift register mutation trigger / cycle.
- **Rotary Encoder**: Incremental encoder on GP2/GP3 with integrated push button on GP6 (`EncoderSwitch` / 1s hold-to-save preset).
- **OLED Display**: SSD1306 over I2C0 (GP4 SDA, GP5 SCL) at 1 MHz (Fast-Mode Plus).

## Core Modules
- `include/core/phase_lfo.hpp`: 32-bit phase accumulator, 256-entry Sine LUT, Triangle, Square, S&H, Turing Mutation.
- `include/core/clock_manager.hpp`: Tap tempo (20–300 BPM), MIDI 24 PPQN parser, 1s internal fallback timeout.
- `include/core/midi_router.hpp`: 4-channel CC routing (channels 1–16, CC 0–127).
- `include/core/macro_engine.hpp`: Multi-destination weighted expression pedal scaling matrix.
- `include/core/modulation_engine.hpp`: Top-level aggregation of 4 LFOs, nested cross-modulation, macro scaling, clock, and router.
- `include/core/core0_task.hpp`: 1000 Hz real-time interrupt loop, ADC sampling, and MIDI dispatcher.
- `include/core/core1_task.hpp`: Asynchronous UI task with 30 FPS render throttle.
- `include/ui/ui_controller.hpp`: 4-state visual UI menu machine and preset serialization.

## HAL Layer
- `include/hal/hal_midi.hpp`, `pico_hal_midi.hpp`, `mock_hal_midi.hpp`: Hardware UART & simulation mock.
- `include/hal/hal_gpio.hpp`, `pico_hal_gpio.hpp`, `mock_hal_gpio.hpp`: Tactile button & encoder abstraction.
- `include/hal/hal_adc.hpp`, `pico_hal_adc.hpp`, `mock_hal_adc.hpp`: Analog expression pedal ADC abstraction.
- `include/hal/hal_display.hpp`, `pico_hal_display.hpp`, `mock_hal_display.hpp`: 1 MHz I2C OLED display abstraction.
- `include/hal/hal_flash.hpp`, `pico_hal_flash.hpp`, `mock_hal_flash.hpp`: Preset serialization & SPI NOR flash abstraction.
