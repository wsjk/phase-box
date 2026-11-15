# Phase Box RP2040 Hardware & Pinout Reference

## Pinout Mapping (`include/hal/pinout.hpp`)

All hardware pins and detent constants are centralized in `include/hal/pinout.hpp`.

```cpp
namespace phasebox::hal::pins {
constexpr uint8_t MIDI_TX    = 0;   // uart0 TX, 31,250 baud MIDI out (DIN/TRS)
constexpr uint8_t MIDI_RX    = 1;   // uart0 RX (reserved; MIDI in is not used yet)
constexpr uint8_t ENC_A      = 2;   // rotary encoder phase A
constexpr uint8_t ENC_B      = 3;   // rotary encoder phase B
constexpr uint8_t OLED_SDA   = 4;   // I2C0 SDA
constexpr uint8_t OLED_SCL   = 5;   // I2C0 SCL
constexpr uint8_t ENC_SW     = 6;   // encoder push switch
constexpr uint8_t EXP_ENC_A  = 7;   // expression encoder phase A
constexpr uint8_t EXP_ENC_B  = 8;   // expression encoder phase B
constexpr uint8_t EXP_ENC_SW = 9;   // expression encoder push switch (freeze)
constexpr uint8_t FS_TAP     = 10;  // footswitch 1 (tap / phase reset)
constexpr uint8_t FS_MUTATE  = 11;  // footswitch 2 (mutate)
constexpr uint8_t FS_BYPASS  = 12;  // footswitch 3 (bypass)
}
```

Free GPIO for future controls: GP13–GP22, GP26–GP28 (GP23–GP25 and GP29 are used on the Pico board itself).

## Encoder Detent & Polarity Tuning

| Macro | Default | Effect |
| :--- | :--- | :--- |
| `ENC_TRANSITIONS_PER_DETENT` | `4` | Number of quadrature state changes per physical detent click (typically 4 for Bourns/ALPS; set to 2 for some optical models). Shared by both encoders. |
| `ENC_REVERSED` | `false` | Invert the value encoder (set to `true` if clockwise turns decrease values). |
| `EXP_ENC_REVERSED` | `false` | Invert the expression encoder. |

## Switch Configuration

* All switches (`ENC_SW`, `EXP_ENC_SW`, `FS_TAP`, `FS_MUTATE`, `FS_BYPASS`) are wired **normally-open to ground**.
* RP2040 internal pull-ups are enabled on all input pins (`gpio_pull_up()`).
* Software debouncing uses a 20ms transition filter (`include/hal/debounce.hpp`).

## Expression Encoder

* A second quadrature encoder with push switch: A on GP7, B on GP8, switch on GP9 (freeze on / off), common pins to GND.
* Decoded on Core 0 by its own `QuadratureDecoder` in `DeviceApp::poll_inputs()`; each detent calls `PhaseBoxState::turn_expression()`.
* Value range 0.0–1.0 in 20 clicks (`PhaseBoxState::EXPRESSION_CLICKS`), clamped at both ends. Depth multiplier = $0.5 + 0.5E$.
* The unit has no analog input. `hal_adc.hpp` / `pico_hal_adc.hpp` remain only for the legacy `include/core/` prototypes.

## Control Gestures (`DeviceApp::poll_inputs()`)

* Value encoder pressed + turned: previous / next preset slot (`PresetSession::select_slot`, stops at 1 and 8). The press is then used up (`PresetSession::cancel_press`): no hold-save, no page change.
* `FS_TAP` + `FS_MUTATE` held together for `RESET_HOLD_MS` (2 s): undo the last reset if nothing changed since, otherwise reset to defaults. The tap's BPM effect is reverted when the combo starts.

## Preset Flash (`include/FlashPresetStorage.hpp`, `firmware/include/PicoFlash.hpp`)

* Last 64 KB of flash (`PicoFlash::BASE` = `PICO_FLASH_SIZE_BYTES` - 64 KB); `available()` is false if the image reaches it.
* 2 sectors per slot, 8 x 512-byte records per sector (`seq | len | data | CRC`). Newest valid `seq` wins; the other sector is erased only when the current one is full.
* Erase/program go through `flash_safe_execute()`, which pauses Core 1 (`flash_safe_execute_core_init()` in `core1_entry`).

## Dual-Core Concurrency & Lock-Free IPC

Display frame transfer uses `SeqlockBuffer<FRAME_BYTES>`:
* **Core 0 (Writer)**:
  Increments sequence counter to odd, writes pixel buffer, increments to even with release barrier.
* **Core 1 (Reader)**:
  Reads sequence counter, copies buffer, verifies counter matches and remains even with acquire barrier.
  If a write interrupted the read, the read retries immediately without blocking.
