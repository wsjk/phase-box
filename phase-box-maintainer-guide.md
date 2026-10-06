# Phase Box Developer & Maintainer Guide

An architectural reference, C++ code walkthrough, and feature maintenance manual for Python programmers maintaining and extending the **Phase Box** algorithmic video synthesis modulation engine.

---

## 1. System Architecture & Operating Principles

**Phase Box** is a custom hardware unit built around the Raspberry Pi Pico (RP2040 dual-core ARM Cortex-M0+ microcontroller) housed in a Hammond 1590BB die-cast aluminum enclosure. Its core job is to generate real-time, non-repeating, and rhythmically synchronized **MIDI Control Change (CC)** parameter streams to modulate external video synthesizers (such as *Auto Waaaves* and *r_e_c_u_r*).

### Power & Signal Topology

*   **Power Supply**: Operates on standard 9V DC pedalboard power, stepped down internally to 5V via a dedicated buck regulator connected to the RP2040 `VSYS` pin. Galvanic isolation prevents ground loops and digital noise in video synthesis signals.
*   **Microcontroller Execution**: Dual-core RP2040 running an asymmetric, decoupled task architecture.
*   **Control Dispatches**: Hardware UART0 (GPIO 0 / TX) transmitting 31,250 baud MIDI serial streams.

### Hardware Execution Pipeline

1.  **Hardware Power Input**: 9V DC Pedalboard Power -> Buck Regulator -> 5V VSYS Power to RP2040.
2.  **Core 0 (1 kHz Hardware Timer Interrupt)**: ClockManager Auto-Sync -> PhaseLFO Accumulators -> MacroEngine Scaling -> MidiRouter Generation -> UART0 MIDI Dispatches.
3.  **Core 1 (Background UI & File Thread)**: Rotary Encoder / Buttons -> SSD1306 OLED Framebuffer (30 FPS Throttle) -> LittleFS NOR Flash Preset Operations ("Hold-to-Save").

### Dual-Core Task Division

The RP2040 provides two 32-bit ARM Cortex-M0+ processing cores. To prevent graphics rendering or file system memory operations from causing modulation jitter or MIDI clock drift, Phase Box implements an asymmetric dual-core execution model:

*   **Core 0 (High Priority / Deterministic Real-Time)**:
    *   Runs a hardware timer interrupt strictly every **1,000 microseconds (1 kHz)**.
    *   Updates 4 independent 32-bit phase accumulators (`PhaseLFO`).
    *   Tracks incoming 24 PPQN MIDI clock pulses and calculates tap tempo intervals (`ClockManager`).
    *   Applies expression pedal ADC scaling matrices (`MacroEngine`).
    *   Dispatches 3-byte MIDI Control Change packets over hardware UART (`MidiRouter`).
*   **Core 1 (Background Priority / Asynchronous UI & Storage)**:
    *   Renders visual telemetry, waveform graphics, and menu parameters to a 128x32 SSD1306 OLED display over I2C, throttled to 30 FPS.
    *   Monitors physical rotary encoder rotation, push-button triggers, and footswitch state machines.
    *   Executes preset serialization and NOR flash writes using the **LittleFS** file system when a 1-second long-press ("Hold-to-Save") gesture is triggered on the encoder. Isolating flash writes to Core 1 eliminates micro-stutters on Core 0 during live performance.

---

## 2. Python Developer's Rosetta Stone (C++ Mental Models)

If your primary background is in Python, C++ embedded firmware introduces fundamental differences in memory management, data types, and compile-time execution. The table below maps C++ embedded concepts directly to their Python equivalents.

### Language Concept Comparison

| Concept | Python Paradigm | Embedded C++ Paradigm | Why It Matters in Phase Box |
| :--- | :--- | :--- | :--- |
| **Data Types** | Dynamic `int` (arbitrary size), `float` | Fixed-width integers (`uint8_t`, `uint32_t`), single-precision `float` | Embedded memory is measured in kilobytes. Fixed bits prevent overflow bugs. |
| **Containers** | Dynamic `list` (`[1, 2, 3]`) | `std::vector<T>` or `std::array<T, N>` | `std::array` has fixed size known at compile time. `std::vector` grows dynamically. |
| **Code Files** | Single `.py` module file | Header (`.hpp`) + Source (`.cpp`) | `.hpp` defines the blueprint/interface; `.cpp` provides the implementation. |
| **Data Classes** | `@dataclass` or `dict` | `struct` or `class` | `struct` groups variables together in contiguous memory without overhead. |
| **Memory Allocation**| Automatic Garbage Collection | Stack allocation (default) or Heap (`new`/`delete`) | Heap allocation is avoided in real-time loops to prevent unexpected latency spikes. |
| **Variable Passing**| Implicit object reference | Pass by Value (`T`) vs Pass by Reference (`T&`) | `T&` avoids copying heavy objects and allows functions to modify original variables. |
| **Math Execution**| High-level float operations | Fixed-point integer math & Look-Up Tables (LUT) | RP2040 Cortex-M0+ lacks a hardware Floating-Point Unit (FPU). Integers are faster. |

### Code Side-by-Side Equivalents

#### 1. Header Files (`.hpp`) vs Source Files (`.cpp`)
In Python, importing a file imports both declarations and executable code. In C++, class interfaces are declared in header files (`.hpp`), while implementation logic is written in `.cpp` files.

**Python Equivalent:**
```python
# phase_lfo.py
class PhaseLFO:
    def __init__(self):
        self.phase_accumulator: int = 0
    
    def update(self) -> int:
        self.phase_accumulator += 1000
        return (self.phase_accumulator >> 24) & 0xFF
```

**C++ Interface Header (`include/core/phase_lfo.hpp`):**
```cpp
#pragma once // Prevents file from being included multiple times
#include <cstdint>

namespace phasebox::core {
class PhaseLFO {
public:
    PhaseLFO(); // Constructor declaration
    uint8_t update(); // Method declaration
private:
    uint32_t phase_accumulator_{0}; // Member variable initialized to 0
};
}
```

#### 2. Vector Access vs Attribute Access (The #1 Common Pitfall)
In Python, if a function returns a list of objects, you index into the element before accessing properties: `items[0].value`. In C++, attempting to access object attributes directly on a `std::vector` container raises a compile error.

**Python:**
```python
messages = engine.tick(1000)
status_code = messages[0].status  # Access element 0
```

**C++:**
```cpp
// WRONG (Causes compile error: "no member named 'status' in std::vector")
std::vector<MidiMessage> msgs = engine.tick(1000);
uint8_t s = msgs.status; 

// CORRECT
std::vector<MidiMessage> msgs = engine.tick(1000);
uint8_t s = msgs[0].status; // Index into element 0 first
```

#### 3. References (`&`) and Value Semantics
In Python, passing a mutable object to a function passes a reference automatically. In C++, passing `T` creates a full memory copy, whereas passing `T&` passes a reference to the existing instance.

```cpp
// Modifies the original clock instance directly without creating a copy
void update_system_tempo(ClockManager& clock, float new_bpm) {
    clock.set_bpm(new_bpm);
}
```

---

## 3. Core DSP & Signal Processing Modules

The Phase Box core logic is organized into clean, isolated C++ classes located under `include/core/`.

### Component Interconnection Pipeline

1.  **`ClockManager`**: Processes footswitch tap tempo or 24 PPQN external MIDI clock pulses -> Outputs system BPM.
2.  **`PhaseLFO`**: Consumes system BPM -> Advances 32-bit phase accumulator -> Evaluates waveform LUT -> Outputs 7-bit integer value ($0–127$).
3.  **`MidiRouter`**: Consumes 7-bit LFO value -> Formats status byte, CC parameter number, and 7-bit data byte -> Outputs 3-byte `MidiMessage` struct.
4.  **`ModulationEngine`**: Aggregates 4x `PhaseLFO` instances, `ClockManager`, and `MidiRouter` -> Dispatches message vector on every 1000 Hz timer tick.

### 1. `PhaseLFO` (`include/core/phase_lfo.hpp`)

`PhaseLFO` generates low-frequency modulation waveforms using a 32-bit phase accumulator.

#### How Phase Accumulation Works
Instead of calculating expensive trigonometric functions (`std::sin()`) every millisecond, the LFO maintains a 32-bit unsigned integer `phase_accumulator_`. Every tick, a phase increment value (`phase_increment_`) is added to it:

$$\text{phase\_accumulator\_} = \text{phase\_accumulator\_} + \text{phase\_increment\_}$$

When the 32-bit integer overflows past $2^{32}-1$ ($4,294,967,295$), it wraps back around to $0$ automatically. The top 8 bits ($\text{phase\_accumulator\_} \gg 24$) are extracted to yield an index from $0$ to $255$, which indexes into a pre-calculated 256-entry Sine Look-Up Table (LUT).

$$\text{LUT\_index} = \frac{\text{phase\_accumulator\_}}{2^{24}}$$

#### Supported Waveforms
*   `Waveform::Sine`: Evaluates 256-entry sine table mapped to 7-bit output ($0–127$).
*   `Waveform::Triangle`: Derived mathematically from the phase accumulator index.
*   `Waveform::Square`: Outputs $127$ when phase $< 2^{31}$, and $0$ when phase $\ge 2^{31}$.
*   `Waveform::SampleAndHold`: Generates a pseudo-random 7-bit value on phase wrap-around.
*   `Waveform::TuringMutation`: Linear Feedback Shift Register (LFSR) stepped on phase wrap-around with configurable mutation probability.

---

### 2. `ClockManager` (`include/core/clock_manager.hpp`)

`ClockManager` provides dual clock synchronization, allowing Phase Box to switch seamlessly between internal tempo and external gear.

*   **Tap Tempo**: Measures microseconds between footswitch presses. Calculates tempo bounded within $20.0 \text{ BPM}$ and $300.0 \text{ BPM}$.
*   **External MIDI Clock Auto-Sync**: Monitors incoming 24 PPQN (Pulse Per Quarter Note) MIDI clock bytes (`0xF8`). When 24 clock pulses arrive, it computes the exact elapsed time and locks system tempo to the external master.
*   **Fallback Mechanism**: If no external MIDI clock bytes arrive for $1,000,000 \text{ \mu s}$ ($1 \text{ second}$), `ClockManager` automatically switches `ClockSource` back to `InternalTap` without interrupting output.

---

### 3. `MidiRouter` (`include/core/midi_router.hpp`)

`MidiRouter` converts raw LFO integer values ($0–127$) into standard 3-byte MIDI Control Change (CC) messages:

```cpp
struct MidiMessage {
    uint8_t status; // Byte 1: Command & Channel (e.g., 0xB0 = CC on Channel 1)
    uint8_t data1;  // Byte 2: CC Parameter Number (0-127)
    uint8_t data2;  // Byte 3: CC Value (0-127)
};
```

*   **Status Byte Calculation**: For Control Change messages, the upper nibble is `0xB0`. The lower nibble contains the zero-indexed MIDI channel ($0–15$, representing MIDI channels $1–16$):

$$\text{status} = 0\text{xB0} \mid (\text{channel} - 1)$$

---

### 4. `ModulationEngine` (`include/core/modulation_engine.hpp`)

`ModulationEngine` is the master controller on Core 0. It contains an array of four `PhaseLFO` instances, a `ClockManager`, and a `MidiRouter`.

```cpp
std::vector<MidiMessage> tick(uint32_t current_time_us) {
    clock_manager_.update(current_time_us);
    std::vector<MidiMessage> messages;
    messages.reserve(4);

    for (size_t i = 0; i < lfos_.size(); ++i) {
        uint8_t val = lfos_[i].update();
        messages.push_back(router_.generate_cc_message(static_cast<uint8_t>(i), val));
    }

    return messages;
}
```

---

## 4. Hardware Abstraction Layer (HAL) & Test-Driven Build System

Phase Box uses a **Hardware Abstraction Layer (HAL)** to decouple hardware hardware calls (RP2040 SDK) from core DSP logic.

### Hardware Abstraction Architecture

*   **Core C++ Engine Logic**: `PhaseLFO`, `ClockManager`, `MidiRouter`, `ModulationEngine`.
*   **HAL Abstract Interfaces**: `include/hal/hal_gpio.hpp`, `include/hal/hal_uart.hpp`.
*   **Build Target 1 (Host Simulation)**: Links against Linux/macOS GCC/Clang and Google Test (`gtest`) in `build-host/`.
*   **Build Target 2 (Pico Target)**: Links against `arm-none-eabi-gcc` and the Raspberry Pi Pico SDK (`pico-sdk`) in `build-pico/`.

---

## 5. Maintenance & Feature Addition Guides

### Guide A: How to Add a New Waveform to `PhaseLFO`

Suppose you want to add a new **Sawtooth (Ramp Up)** waveform to Phase Box.

#### Step 1: Update Enum in `include/core/phase_lfo.hpp`
Add `Sawtooth` to the `Waveform` enum class:

```cpp
enum class Waveform {
    Sine,
    Triangle,
    Square,
    SampleAndHold,
    TuringMutation,
    Sawtooth // <--- Add new entry
};
```

#### Step 2: Implement Calculation in `PhaseLFO::update()`
In `include/core/phase_lfo.hpp`, update the `switch(waveform_)` block to handle `Sawtooth`:

```cpp
case Waveform::Sawtooth: {
    // Map 32-bit accumulator (0 to 4294967295) directly to 7-bit value (0 to 127)
    output_val = static_cast<uint8_t>(phase_accumulator_ >> 25);
    break;
}
```

#### Step 3: Write Unit Test in `tests/test_main.cpp`
Add a Google Test case to verify waveform output:

```cpp
TEST(PhaseLFOTest, WaveformSawtooth) {
    PhaseLFO lfo;
    lfo.set_waveform(Waveform::Sawtooth);
    lfo.set_frequency(1.0f, 1000);

    uint8_t val_initial = lfo.update();
    EXPECT_GE(val_initial, 0);
    EXPECT_LE(val_initial, 127);
}
```

#### Step 4: Run Tests
Run the build script to confirm your new waveform compiles and passes all unit tests:

```bash
cd build-host && make && ./tests/run_host_tests
```

---

### Guide B: How to Add a New Unit Test in Google Test

When adding new engine logic or fixing bugs:

1. Open `tests/test_main.cpp`.
2. Write a new `TEST(TestSuiteName, TestName)` block.
3. Use GTest macros for assertions:
   * `EXPECT_EQ(a, b)`: Asserts $a == b$.
   * `EXPECT_NEAR(a, b, max_abs_diff)`: Asserts floating-point equality within a tolerance window.
   * `EXPECT_GE(a, b)` / `EXPECT_LE(a, b)`: Asserts greater-than-or-equal / less-than-or-equal.

```cpp
TEST(MidiRouterTest, CustomCCAssignment) {
    MidiRouter router;
    router.set_target_channel(2, 5); // Set LFO Index 2 to MIDI Channel 5
    router.set_target_cc(2, 74);     // Set LFO Index 2 to CC #74 (Brightness)

    MidiMessage msg = router.generate_cc_message(2, 120);
    EXPECT_EQ(msg.status, 0xB4);     // 0xB0 | (5 - 1) = 0xB4
    EXPECT_EQ(msg.data1, 74);       // CC Parameter #74
    EXPECT_EQ(msg.data2, 120);      // Value 120
}
```
