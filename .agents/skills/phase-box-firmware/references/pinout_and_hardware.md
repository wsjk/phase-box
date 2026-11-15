# Phase Box RP2040 Hardware & Pinout Reference

## Pinout Mapping (`include/hal/pinout.hpp`)

All hardware pins and detent constants are centralized in `include/hal/pinout.hpp`.

```cpp
namespace phasebox::hal::pins {
    constexpr uint8_t UART_TX    = 0;   // GP0: 31,250 baud MIDI out (PicoHalMidi default)
    constexpr uint8_t UART_RX    = 1;   // GP1: reserved for MIDI in (not read yet)
    constexpr uint8_t ENC_A      = 2;   // GP2: Quadrature encoder A
    constexpr uint8_t ENC_B      = 3;   // GP3: Quadrature encoder B
    constexpr uint8_t OLED_SDA   = 4;   // GP4: SSD1306 I2C0 SDA
    constexpr uint8_t OLED_SCL   = 5;   // GP5: SSD1306 I2C0 SCL
    constexpr uint8_t ENC_SW     = 6;   // GP6: Encoder pushbutton
    constexpr uint8_t EXP_ENC_A  = 7;   // GP7: Expression encoder A
    constexpr uint8_t EXP_ENC_B  = 8;   // GP8: Expression encoder B
    constexpr uint8_t FS_TAP     = 10;  // GP10: Tap tempo footswitch
    constexpr uint8_t FS_MUTATE  = 11;  // GP11: Mutate footswitch
}
```

## Encoder Detent & Polarity Tuning

| Macro | Default | Effect |
| :--- | :--- | :--- |
| `ENC_TRANSITIONS_PER_DETENT` | `4` | Number of quadrature state changes per physical detent click (typically 4 for Bourns/ALPS; set to 2 for some optical models). Shared by both encoders. |
| `ENC_REVERSED` | `false` | Invert the value encoder (set to `true` if clockwise turns decrease values). |
| `EXP_ENC_REVERSED` | `false` | Invert the expression encoder. |

## Switch Configuration

* All switches (`ENC_SW`, `FS_TAP`, `FS_MUTATE`) are wired **normally-open to ground**.
* RP2040 internal pull-ups are enabled on all input pins (`gpio_pull_up()`).
* Software debouncing uses a 20ms transition filter (`include/hal/debounce.hpp`).
* The internal pull-ups are weak (~50 kΩ). For long footswitch leads, add 10 kΩ to 3V3 and 100 nF to GND per line.

## MIDI Wiring

* MIDI out at 3.3 V: 33 Ω from 3V3 to DIN pin 4, 10 Ω from GP0 to DIN pin 5 (220 Ω values are for 5 V senders). TRS Type A and Type B swap these two contacts.
* MIDI in (GP1) is not implemented. It needs an optocoupler (6N138 / H11L1); never connect a MIDI jack directly to GP1.
* OLED I2C runs at 400 kHz (the SSD1306 rating). `-DPHASEBOX_OLED_I2C_HZ=1000000` is out of spec and only for modules known to cope.

## Expression Encoder

* A second quadrature encoder (no push switch): A on GP7, B on GP8, common pin to GND.
* Decoded on Core 0 by its own `QuadratureDecoder` in `DeviceApp::poll_inputs()`; each detent calls `PhaseBoxState::turn_expression()`.
* Value range 0.0–1.0 in 20 clicks (`PhaseBoxState::EXPRESSION_CLICKS`), clamped at both ends. Depth multiplier = $0.5 + 0.5E$.
* The unit has no analog input. `hal_adc.hpp` / `pico_hal_adc.hpp` remain only for the legacy `include/core/` prototypes.

## Dual-Core Concurrency & Lock-Free IPC

Display frame transfer uses `SeqlockBuffer<FRAME_BYTES>`:
* **Core 0 (Writer)**:
  Increments sequence counter to odd, writes pixel buffer, increments to even with release barrier.
* **Core 1 (Reader)**:
  Reads sequence counter, copies buffer, verifies counter matches and remains even with acquire barrier.
  If a write interrupted the read, the read retries immediately without blocking.
