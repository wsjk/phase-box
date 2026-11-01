# Phase Box 🎛️

> **RP2040-Powered Polyrhythmic Modulation Engine & MIDI Synthesizer**

Phase Box is a dedicated hardware modulation engine and MIDI generator built on the Raspberry Pi RP2040. It translates algorithmic phase accumulators, generative LFSR Turing machine registers, and polyrhythmic macro scaling into real-time MIDI Control Change (CC) streams over both **DIN/TRS 31,250 baud serial MIDI** and **class-compliant USB-MIDI**.

Designed specifically for controlling video synthesizers (such as LZX Industries, Auto Waaaves, and r_e_c_u_r), modular audio rigs, and DAWs.

---

## 📚 Documentation Quick Links

* 📖 **[User Guide & Operating Manual](docs/USER_GUIDE.md)**: Hardware controls, menu pages, tap tempo, footswitches, expression pedal, screensaver, and desktop simulator guide.
* 🛠️ **[Developer & Architecture Guide](docs/DEVELOPER_GUIDE.md)**: Dual-core architecture, signal flow diagrams, **"C++ Crash Course for Python Programmers"**, and step-by-step contribution recipes.
* 📐 **[Context Handover Blueprint](docs/phase_box_context_handover_blueprint.md)**: Complete implementation state and execution phases.

---

## ⚡ Key Features

- **Asymmetric Dual-Core Architecture:**
  - **Core 0 (1000 Hz Hard Real-Time):** Phase accumulators, 4-channel LFO calculation, nested cross-modulation, Turing LFSR mutation, ADC expression sampling, and MIDI packet streaming.
  - **Core 1 (Async UI & Storage):** SSD1306 128×32/64 OLED display at 30–60 FPS, rotary encoder debouncing, OLED screen burn-in protection, and non-volatile flash auto-save.
- **Polyrhythmic Macro Engine:** Independent clock multipliers/subdivisions per CC destination ($1/4\times$, $1/2\times$, $1\times$, $1.5\times$, $2\times$, etc.) for generative, non-repeating modulation patterns.
- **Bipolar Expression Scaling:** Full expression pedal integration with EMA smoothing and deadband clamping.
- **Dual MIDI Output:** Hardware serial MIDI (31,250 baud on GP0) and plug-and-play USB-MIDI class compliance (TinyUSB).
- **Desktop Simulator:** Native Raylib desktop visualizer connecting directly to macOS `CoreMIDI`.
- **Zero-Hardware Testing:** Fully decoupled Hardware Abstraction Layer (HAL) with a GoogleTest host test suite (`tests/`, 56 tests).

---

## 🔌 Hardware Pinout

| Peripheral | RP2040 Pin | Hardware Role |
| :--- | :--- | :--- |
| **MIDI Serial** | **GP0** (TX), **GP1** (RX) | `uart0` running at 31,250 baud (8N1). |
| **Rotary Encoder** | **GP2** (A), **GP3** (B), **GP6** (Switch) | Navigation, velocity-accelerated tuning, hold-to-save. |
| **OLED Display** | **GP4** (SDA), **GP5** (SCL) | `i2c0` at 400 kHz / 1 MHz (Address: `0x3C`). |
| **Footswitch 1** | **GP10** | Tap tempo (20–300 BPM) & instant phase reset. |
| **Footswitch 2** | **GP11** | Turing shift register mutation trigger / cycle. |
| **Expression Pedal**| **GP26** (ADC0) | Analog 0.0V–3.3V input with software EMA filtering. |
| **USB** | **USB D+ / D-** | Class-compliant TinyUSB MIDI device streaming. |

---

## 🚀 Quick Start Workflows

### 1. Run the Desktop Simulator (macOS)
```bash
./build-native/phase_box_native_sim
```
*Creates a virtual MIDI device called **`Phase Box Virtual Out`** that instantly connects to Ableton, Logic, VCV Rack, or hardware synths.*

### 2. Run Host Unit Tests (GoogleTest)
```bash
/Applications/CMake.app/Contents/bin/cmake -S tests -B build-test -DCMAKE_POLICY_VERSION_MINIMUM=3.5
/Applications/CMake.app/Contents/bin/cmake --build build-test -j4
./build-test/run_host_tests
```

### 3. Compile RP2040 Target Firmware (`.uf2`)
```bash
export PATH="/Applications/CMake.app/Contents/bin:/Users/ww/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin:$PATH"
export PICO_SDK_PATH="/Users/ww/tools/pico-sdk"
cmake --build firmware/build -j4
```
*Drag-and-drop `firmware/build/phase_box_firmware.uf2` to your Raspberry Pi Pico in BOOTSEL mode.*
