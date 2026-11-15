# Phase Box RP2040 Hardware & Pinout Reference

## Pinout Mapping (`include/hal/pinout.hpp`)

All hardware pins and detent constants are centralized in `include/hal/pinout.hpp`.

```cpp
namespace phasebox::hal::pins {
    constexpr unsigned UART_TX     = 0;  // GP0: 31,250 baud MIDI serial
    constexpr unsigned UART_RX     = 1;  // GP1: MIDI serial RX
    constexpr unsigned ENC_A       = 2;  // GP2: Quadrature encoder A
    constexpr unsigned ENC_B       = 3;  // GP3: Quadrature encoder B
    constexpr unsigned OLED_SDA    = 4;  // GP4: SSD1306 I2C0 SDA
    constexpr unsigned OLED_SCL    = 5;  // GP5: SSD1306 I2C0 SCL
    constexpr unsigned ENC_SW      = 6;  // GP6: Encoder pushbutton
    constexpr unsigned EXP_ENC_A   = 7;  // GP7: Expression encoder A
    constexpr unsigned EXP_ENC_B   = 8;  // GP8: Expression encoder B
    constexpr unsigned FS_TAP      = 10; // GP10: Tap tempo footswitch
    constexpr unsigned FS_MUTATE   = 11; // GP11: Mutate footswitch
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
