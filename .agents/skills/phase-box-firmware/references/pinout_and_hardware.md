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
    constexpr unsigned FS_TAP      = 10; // GP10: Tap tempo footswitch
    constexpr unsigned FS_MUTATE   = 11; // GP11: Mutate footswitch
    constexpr unsigned EXPR_PEDAL  = 26; // GP26: ADC0 Expression analog input
}
```

## Encoder Detent & Polarity Tuning

| Macro | Default | Effect |
| :--- | :--- | :--- |
| `ENC_TRANSITIONS_PER_DETENT` | `4` | Number of quadrature state changes per physical detent click (typically 4 for Bourns/ALPS; set to 2 for some optical models). |
| `ENC_REVERSED` | `0` | Invert rotation direction (set to 1 if clockwise turns decrease values). |

## Switch Configuration

* All switches (`ENC_SW`, `FS_TAP`, `FS_MUTATE`) are wired **normally-open to ground**.
* RP2040 internal pull-ups are enabled on all input pins (`gpio_pull_up()`).
* Software debouncing uses a 20ms transition filter (`include/hal/debounce.hpp`).

## ADC & Expression Circuit

* Expression pedal connects via a 1/4" stereo TRS jack:
  * **Tip**: Wiper connected to GP26 (ADC0).
  * **Ring**: Reference voltage (3.3V RP2040 supply).
  * **Sleeve**: Ground (GND).
* Software EMA filter:
  $$V_{\text{filtered}} = \alpha \cdot V_{\text{sample}} + (1 - \alpha) \cdot V_{\text{prev}}$$
* Lower and upper deadbands (typically 2% of range) clamp noise at extremes to guarantee exact $0.0$ and $1.0$.

## Dual-Core Concurrency & Lock-Free IPC

Display frame transfer uses `SeqlockBuffer<FRAME_BYTES>`:
* **Core 0 (Writer)**:
  Increments sequence counter to odd, writes pixel buffer, increments to even with release barrier.
* **Core 1 (Reader)**:
  Reads sequence counter, copies buffer, verifies counter matches and remains even with acquire barrier.
  If a write interrupted the read, the read retries immediately without blocking.
