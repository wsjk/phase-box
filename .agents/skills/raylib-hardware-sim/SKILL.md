---
name: raylib-hardware-sim
description: >-
  Blueprint and implementation guide for building desktop hardware simulators, stompbox
  enclosures, synth control panels, and embedded device UIs using C++ and Raylib.
  Use when creating a native desktop visualizer, interactive physical controls (knobs,
  sliders, footswitches, OLED/LCD emulators), or testing embedded device state machines.
---

# Raylib Hardware Simulator Skill

This skill provides a reusable architectural blueprint, design patterns, and ready-to-use template code for building realistic desktop simulators for physical hardware devices (guitar pedals, synthesizers, Eurorack modules, IoT appliances, microcontrollers) using **C++17** and **Raylib**.

---

## 🏗️ The 3-Tier Architecture Pattern

Building a robust hardware simulator requires cleanly decoupling **physical geometry** and **interaction math** from the **graphics library**. This allows the entire UI and layout to be unit-tested in headless CI environments without requiring a window server.

```
+-------------------------------------------------------------+
| TIER 1: PURE GEOMETRY (Layout.hpp)                          |
| - Zero Raylib/OpenGL dependencies                           |
| - Defines Rect, Circle, hit_rect(), hit_circle(), overlap() |
| - 100% testable in headless GoogleTest / Catch2             |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
| TIER 2: MECHANICAL CONTROLS & MATH (Controls.hpp)           |
| - KnobDrag: Circular drag around rim vs vertical drag       |
| - Potentiometers: 270° sweep, end stops, detent snapping    |
| - Sliders / Treadles: 1D linear mapping to [0.0, 1.0]       |
| - Footswitches & Hold Timers: Press depression & hold rings |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
| TIER 3: RAYLIB IMMEDIATE-MODE RENDERER (sim_main.cpp)       |
| - 60 FPS game loop: Input polling -> State step -> Draw     |
| - Visual skeuomorphism: Drop shadows, metal bevels, LEDs    |
| - Pixel-scaled OLED / LCD / 7-Segment screen rasterizers   |
+-------------------------------------------------------------+
```

---

## 🚀 Quick Start: Creating a New Simulator Project

### 1. Minimal `CMakeLists.txt` (Zero System Dependencies)

Raylib can be automatically downloaded and compiled at configure time using CMake's `FetchContent`. This ensures your simulator builds identically on macOS, Linux, and Windows:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_hardware_sim LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.0.tar.gz
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

add_executable(my_sim src/main.cpp)
target_include_directories(my_sim PRIVATE include)
target_link_libraries(my_sim PRIVATE raylib)

if (APPLE)
    target_link_libraries(my_sim PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()
```

---

## 🎛️ Reusable Control Blueprints

See [architecture_pattern.md](./references/architecture_pattern.md) and [visual_styling_guide.md](./references/visual_styling_guide.md) for full implementation details.

### 1. The Interactive Rotary Knob (`KnobDrag`)
A physical potentiometer knob requires two mouse interaction styles:
* **Circular Drag:** Dragging tangentially around the outer rim turns the knob (clockwise = up).
* **Vertical Drag:** Clicking inside the central hub ($r < 22\text{px}$) lets the user drag straight up/down like a DAW plugin.
* **Scroll Wheel:** Scrolling over the knob generates discrete detent steps.

### 2. The OLED / LCD Display Emulator
To emulate an embedded $128 \times 64$ monochrome OLED or LCD:
* Maintain a 1-bit or byte buffer in your hardware abstraction layer.
* Render each hardware pixel as an enlarged square on screen (`scale = 2` or `3` pixels).
* Add a recessed glass bezel (`DrawRectangleRounded`) with drop-shadows.

### 3. Clickable Footswitches & Buttons
* Draw a brushed-metal base ring (`DrawCircle` + `DrawCircleLines`).
* On mouse click (`IsMouseButtonDown`), contract the cap radius by 2–4 pixels and shift $Y$ down by 2 pixels to simulate mechanical spring depression.

### 4. Expression Treadles & Sliders
* Track normalized $0.0 \dots 1.0$ travel:
  $$\text{val} = \text{clamp}\left(1.0 - \frac{y - \text{track}.y}{\text{track}.h},\ 0.0,\ 1.0\right)$$
* Fill the track with an illuminated color bar up to the handle position.

---

## 📁 Ready-to-Use Templates in this Skill

This skill includes standalone starter templates in [`templates/`](./templates/):
* [`CMakeLists.txt.template`](./templates/CMakeLists.txt.template): Standalone CMake build configuration with FetchContent.
* [`Layout.hpp.template`](./templates/Layout.hpp.template): Pure geometry layout struct and hit-testing primitives.
* [`KnobDrag.hpp.template`](./templates/KnobDrag.hpp.template): Angular circular/vertical knob drag class.
* [`sim_main.cpp.template`](./templates/sim_main.cpp.template): Complete 60 FPS working pedal simulator with knob, OLED, footswitches, and LED.

---

## 📋 Installing this Skill Globally

To make this skill available across **all** your repositories for Claude, Antigravity, and Gemini:

```bash
# Copy to your global agent skills directory:
mkdir -p ~/.gemini/config/skills/raylib-hardware-sim
cp -r .agents/skills/raylib-hardware-sim/* ~/.gemini/config/skills/raylib-hardware-sim/
```
Once installed globally, any agent will automatically discover it on new projects!
