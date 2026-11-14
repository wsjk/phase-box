# Phase Box: Raylib Desktop UI Guide

> A beginner-friendly guide to understanding, modifying, and extending the Raylib desktop simulator interface for **Phase Box**.

---

## 1. What is Raylib and How Does it Work?

If you are accustomed to web browsers (HTML/DOM), mobile frameworks (UIKit, SwiftUI, Flutter), or desktop widget toolkits (Qt, GTK), Raylib works very differently:

* **No Retained Widget Hierarchy:** There are no `Button` objects, no XML/HTML files, and no `onClick` event listener registrations.
* **Immediate-Mode Rendering:** Raylib runs a 60 Hz loop. **60 times every second**, the window is completely cleared and re-drawn from top to bottom based on the current state.
* **Direct Input Polling:** Rather than waiting for callbacks, you poll input functions (like `IsMouseButtonPressed()` or `GetMousePosition()`) at the start of each frame.

### The Canonical Raylib Loop

```cpp
#include "raylib.h"

int main() {
    InitWindow(800, 600, "My Window");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        // 1. Poll Inputs
        Vector2 mouse = GetMousePosition();
        bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        // 2. Update State
        if (clicked) {
            // handle click...
        }

        // 3. Draw Everything
        BeginDrawing();
            ClearBackground(DARKGRAY);
            DrawText("Hello World", 20, 20, 20, WHITE);
            DrawCircle(mouse.x, mouse.y, 10, RED);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

---

## 2. Raylib Basics Cheat Sheet

### 2.1 Coordinate System

* Origin `(0, 0)` is at the **top-left corner** of the window.
* Positive **X** extends to the **right**.
* Positive **Y** extends **downwards**.
* Window dimensions: `Layout::window_w = 1160`, `Layout::window_h = 800`.

### 2.2 Colors & Alpha Transparency

Raylib represents colors as an 8-bit RGBA struct:
```cpp
Color myColor = Color{ red, green, blue, alpha }; // 0 to 255
```
* **Predefined Colors:** `WHITE`, `BLACK`, `LIGHTGRAY`, `DARKGRAY`, `RED`, `GREEN`, `BLUE`, `ORANGE`, `SKYBLUE`.
* **Alpha Blending (`Fade`):**
  ```cpp
  Fade(Color color, float alpha); // alpha from 0.0f (transparent) to 1.0f (opaque)
  ```

### 2.3 Drawing 2D Primitives

| Raylib Function | Purpose | Example |
| :--- | :--- | :--- |
| `DrawRectangle(x, y, w, h, color)` | Solid rectangle | `DrawRectangle(10, 10, 100, 50, BLUE);` |
| `DrawRectangleRounded(rect, roundness, segments, color)` | Rounded rectangle | `DrawRectangleRounded(Rectangle{10, 10, 100, 50}, 0.2f, 8, DARKGRAY);` |
| `DrawCircle(cx, cy, radius, color)` | Solid filled circle | `DrawCircle(300, 200, 25, RED);` |
| `DrawCircleLines(cx, cy, radius, color)` | Circle wireframe / outline | `DrawCircleLines(300, 200, 25, WHITE);` |
| `DrawLineEx(v1, v2, thickness, color)` | Anti-aliased thick line | `DrawLineEx(Vector2{0,0}, Vector2{100,100}, 3.0f, GREEN);` |
| `DrawRing(center, innerR, outerR, startAngle, endAngle, segs, color)` | Arc or donut ring | Used for the rotary knob's hold-to-save progress bar |

### 2.4 Typography & Text

* In `native_sim_main.cpp`, text is drawn via the embedded font ([SimFont.hpp](../include/SimFont.hpp)):
  ```cpp
  ui_text(const char* text, float x, float y, float size, Color c, bool bold = false);
  centered_text(const char* text, float cx, float y, int size, Color c, bool bold = false);
  int width = ui_measure(text, size, bold);
  ```
* **Formatted Strings:** Use Raylib's `TextFormat("Score: %d", score)` (behaves like `printf`).

### 2.5 Input Polling Functions

| Function | Return Type | Description |
| :--- | :--- | :--- |
| `GetMousePosition()` | `Vector2` | Current mouse cursor `(x, y)`. |
| `IsMouseButtonPressed(btn)` | `bool` | True on the **single frame** mouse button went down. |
| `IsMouseButtonDown(btn)` | `bool` | True on **every frame** mouse button remains held. |
| `IsMouseButtonReleased(btn)`| `bool` | True on the single frame mouse button is let go. |
| `GetMouseWheelMove()` | `float` | Scrolled amount (+1.0 up, -1.0 down). |
| `IsKeyPressed(KEY_*)` | `bool` | True on the frame a key is struck (e.g. `KEY_E`, `KEY_T`). |

---

## 3. Phase Box UI Architecture

The Phase Box simulator decouples **geometry** from **rendering**:

```
+-------------------------------------------------------+
| 1. SimLayout.hpp (Pure Geometry, ZERO Raylib)        |
|    - Defines Rect, Circle, Layout::make()            |
|    - hit_rect(), hit_circle(), overlap()              |
|    - Unit-tested in tests/test_sim_layout.cpp         |
+-------------------------------------------------------+
                           |
+-------------------------------------------------------+
| 2. SimPot.hpp / KnobDrag (Mechanical Behaviors)       |
|    - Translates mouse drag/scroll into clicks/angles  |
|    - Circular drag around rim vs vertical middle drag |
|    - Unit-tested in tests/test_sim_pot.cpp            |
+-------------------------------------------------------+
                           |
+-------------------------------------------------------+
| 3. native_sim_main.cpp (Raylib Rendering Loop)        |
|    - Polls Raylib inputs: GetMousePosition(), etc.    |
|    - Invokes drawing routines: draw_knob, draw_oled...|
|    - Renders at 60 FPS                                |
+-------------------------------------------------------+
```

### Why This Split Matters
Because `SimLayout.hpp` has **no dependencies on Raylib or OpenGL**, you can write and run GoogleTest unit tests on Linux, macOS, or in headless CI servers to verify bounding box positions, click targets, and drag math without needing an active display!

---

## 4. Tour of Visual Components (`src/native_sim_main.cpp`)

### 4.1 The Stompbox Enclosure (`draw_screw`, lines 104–108)
The enclosure is drawn with three layers:
1. Outer drop-shadow rectangle with rounded corners (`kBodyEdge`).
2. Main dark aluminum pedal body (`kBody`).
3. Subtle top-edge bevel highlight for depth (`kBodyLight`).
4. Screws in the four corners: dark circle base + metal cap + angled slot line.

### 4.2 The OLED Screen (`draw_oled`, lines 180–194)
* The display is drawn inside `L.oled_bezel` and `L.oled_screen`.
* It rasterizes the real embedded `FrameBuffer` (128 columns $\times$ 4 or 8 pages of 8 vertical pixels each).
* Each pixel on the RP2040 display is drawn as an enlarged square (`L.oled_scale = 3` screen pixels per OLED pixel).

### 4.3 The Rotary Knob (`draw_knob`, lines 149–178)
* **Shadow and Ring:** Dark outer shadow and detent tick marks around the knob.
  * In potentiometer mode (`pot = true`), tick marks span $270^\circ$ from `simpot::MIN_DEGREES` to `simpot::MAX_DEGREES`.
  * In endless encoder mode, ticks span the entire $360^\circ$ at $15^\circ$ detents.
* **Hold Progress:** When holding the knob, `DrawRing` renders an amber radial arc from $-90^\circ$ around the ring until it reaches $360^\circ$ (1 second).
* **Knob Cap:** Multiple layered circles with brushed metallic concentric rings (`DrawCircleLines`).
* **Pointer Line:** An angled radial indicator showing current rotation.

### 4.4 The Footswitches (`draw_footswitch`, lines 138–147)
* Concentric metallic circles simulating a classic heavy-duty Carling stomp switch.
* When clicked (`pressed = true`), the inner cap radius contracts and drops down by 2 pixels to simulate mechanical spring travel.

### 4.5 The Expression Treadle (`draw_treadle`, lines 202–212)
* Represents an expression pedal connected to the unit.
* A vertical slider track (`L.treadle`).
* The handle position is clamped on its track using `simlayout::y_from_expression()`.
* The region below the handle fills with amber tint proportional to the pedal angle.

### 4.6 The Diagnostic Monitor (lines 468–516)
* The large lower panel displays real-time polyrhythmic output values, active MIDI destination details, CoreMIDI connection state, and a full menu page directory.

---

## 5. Practical Modification Recipes

### Recipe 1: Customizing Colors & Styling

All colors are defined at the top of [src/native_sim_main.cpp](../src/native_sim_main.cpp#L36-L51):

```cpp
const Color kWindowBg   = {18, 19, 24, 255};   // Window backdrop
const Color kPanel      = {28, 30, 38, 255};   // Lower monitor panel
const Color kBody       = {46, 50, 61, 255};   // Stompbox enclosure color
const Color kSilk       = {214, 218, 226, 255};// Silkscreen text (bright)
const Color kAmber      = {255, 176, 46, 255}; // Accent color (potentiometer, highlights)
const Color kCyan       = {96, 205, 255, 255}; // Secondary accent (Turing flash)
```
Change any RGB values and recompile with `cmake --build build-native -j4`.

---

### Recipe 2: Adding a New Shortcut Button

1. **Add button enum** in [include/SimLayout.hpp](../include/SimLayout.hpp#L34):
   ```cpp
   enum Button {
       BTN_FREEZE, BTN_BYPASS, BTN_RESET, BTN_UNDO,
       BTN_SLOT_PREV, BTN_SLOT_NEXT,
       BTN_PANIC, // <-- New button!
       BUTTON_COUNT
   };
   ```
2. **Handle click action** in [src/native_sim_main.cpp](../src/native_sim_main.cpp#L306-L316):
   ```cpp
   case simlayout::BTN_PANIC:
       // Perform action (e.g. send MIDI All Notes Off or phase reset)
       state.restart_phase();
       break;
   ```
3. **Draw the button** in [src/native_sim_main.cpp](../src/native_sim_main.cpp#L461-L467):
   ```cpp
   draw_button(L.buttons[simlayout::BTN_PANIC], "PANIC RESET", false);
   ```
4. **Verify layout fits in window**:
   ```bash
   ./build-test/run_host_tests --gtest_filter="SimLayout.*"
   ```

---

### Recipe 3: Moving or Resizing Controls

All geometric dimensions live in `Layout::make()` in [include/SimLayout.hpp](../include/SimLayout.hpp#L50-L73):

```cpp
static Layout make(int rows) {
    Layout L;
    L.enclosure = {30, 20, 760, 470}; // x, y, width, height
    const float cy = 190;             // shared center line for OLED & Knob
    
    L.knob = {650, cy, 58};           // cx, cy, radius
    L.foot_tap = {262, 380, 46};      // cx, cy, radius
    L.treadle = {830, 60, 80, 300};   // x, y, width, height
    // ...
    return L;
}
```
* If you change coordinates, run `tests/test_sim_layout.cpp` to ensure controls do not intersect or spill outside the window boundaries.

---

### Recipe 4: Adding a New Real-Time Status Metric

To display a new readout in the lower monitor panel, edit the drawing block around line 475 in [src/native_sim_main.cpp](../src/native_sim_main.cpp#L475):

```cpp
// Draw a string at (mx + 15, my + 140) with font size 12 in YELLOW:
ui_text(TextFormat("MY METRIC: %.3f (Units)", my_float_variable), mx + 15, my + 140, 12, YELLOW);
```

---

## 6. Debugging & Verification

* **Fast compilation:**
  ```bash
  cmake -B build-native -S . && cmake --build build-native -j4
  ```
* **Run with virtual MIDI for testing with a DAW:**
  ```bash
  PHASE_BOX_MIDI_DEST=virtual ./build-native/phase_box_native_sim
  ```
* **Run in 32-row OLED mode:**
  ```bash
  PHASE_BOX_OLED_ROWS=32 ./build-native/phase_box_native_sim
  ```
* **Run with endless encoder knob behavior:**
  ```bash
  PHASE_BOX_KNOB=encoder ./build-native/phase_box_native_sim
  ```
