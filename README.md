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
  - **Core 0 (Real-Time Control & MIDI Engine):** Quadrature rotary encoder decoding, switch debouncing, ADC0 expression sampling, 60 Hz modulation engine step, Turing LFSR mutation, bandwidth-managed MIDI CC dispatch over UART (31,250 baud on GP0) and TinyUSB USB-MIDI, and 30 FPS OLED frame rendering into lock-free shared buffers.
  - **Core 1 (Async Display & Screen Saver):** Dedicated SSD1306 128×32/64 OLED driver on `i2c0` offloading all display traffic from Core 0 via lock-free seqlock framebuffers, plus automatic screen burn-in contrast dimming.
- **Polyrhythmic Macro Engine:** Independent clock multipliers/subdivisions per CC destination (0.125× to 8×: 1/8×, 1/4×, 1/2×, 3/4×, 1×, 1.5×, 2×, 3×, 4×, 8×) for generative, non-repeating modulation patterns.
- **Bipolar Expression Scaling:** Full expression pedal integration with EMA smoothing and deadband clamping.
- **Generative Turing LFSR:** 16-bit shift register with probabilistic mutation stepped per master cycle or manual footswitch tap.
- **Dual MIDI Output:** Hardware serial MIDI (31,250 baud on GP0) and plug-and-play USB-MIDI class compliance (TinyUSB).
- **Desktop Simulator:** Native Raylib desktop visualizer connecting directly to macOS `CoreMIDI` (defaulting to `FRASIL FM-1` or virtual DAW port) with live waveform visualization, preset management, freeze, and bypass modes.
- **Zero-Hardware Testing:** Fully decoupled Hardware Abstraction Layer (HAL) with a GoogleTest host test suite (`tests/`, 146 unit tests across 37 test suites).

---

## 🔌 Hardware Pinout

| Peripheral | RP2040 Pin | Hardware Role |
| :--- | :--- | :--- |
| **MIDI Serial** | **GP0** (TX), **GP1** (RX) | `uart0` running at 31,250 baud (8N1). |
| **Rotary Encoder** | **GP2** (A), **GP3** (B), **GP6** (Switch) | Navigation, velocity-accelerated tuning, hold-to-save. |
| **OLED Display** | **GP4** (SDA), **GP5** (SCL) | `i2c0` at 400 kHz / 1 MHz (Address: `0x3C`, 128×32 or 128×64). |
| **Footswitch 1** | **GP10** | Tap tempo (40–300 BPM) & instant phase reset. |
| **Footswitch 2** | **GP11** | Turing shift register mutation trigger / cycle. |
| **Expression Pedal**| **GP26** (ADC0) | Analog 0.0V–3.3V input with software EMA filtering. |
| **USB** | **USB D+ / D-** | Class-compliant TinyUSB MIDI device streaming. |

---

## 🚀 Quick Start Workflows

### 1. Build and Run the Desktop Simulator (macOS)
```bash
cmake -B build-native -S .
cmake --build build-native -j4
./build-native/phase_box_native_sim
```
*Sends MIDI straight to the hardware destination named **`FRASIL FM-1`** (change it with `PHASE_BOX_MIDI_DEST="Some Synth"`; `PHASE_BOX_MIDI_DEST=virtual` gives a virtual port for a DAW). `--list-midi` shows what CoreMIDI sees. If macOS MIDI is stuck, see `bash scripts/reset_coremidi.sh`.*

### 2. Run Host Unit Tests (GoogleTest)
```bash
cmake -S tests -B build-test -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build-test -j4
./build-test/run_host_tests
```
*(Runs all 146 tests covering DSP math, hardware abstraction layers, DeviceApp simulation, preset session persistence, and CoreMIDI routing).*

### 3. Compile RP2040 Target Firmware (`.uf2`)
```bash
export PATH="/Applications/CMake.app/Contents/bin:/Users/ww/tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin:$PATH"
export PICO_SDK_PATH="/Users/ww/tools/pico-sdk"
cmake -S firmware -B firmware/build -DPICO_SDK_PATH="$PICO_SDK_PATH"
cmake --build firmware/build -j4
```
*Drag-and-drop `firmware/build/phase_box_firmware.uf2` to your Raspberry Pi Pico in BOOTSEL mode.*
