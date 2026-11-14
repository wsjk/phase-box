# Hardware Simulator Architecture Pattern & Design Guidelines

## 1. Why Decouple Geometry from Rendering?

In embedded software development, hardware state machines must be rigorously tested. If the UI layout is tightly coupled with OpenGL or Raylib rendering calls, you cannot test:
* Whether controls overlap.
* Whether mouse clicks hit the correct button or knob.
* Whether detent mapping or parameter limits are respected.
* Whether dragging past $180^\circ$ wraps smoothly without jitter.

By creating a pure-geometry `Layout` struct:
```cpp
namespace simlayout {
    struct Rect { float x, y, w, h; };
    struct Circle { float x, y, r; };
    
    inline bool hit_rect(const Rect& r, float px, float py, float margin = 0);
    inline bool hit_circle(const Circle& c, float px, float py, float margin = 0);
    inline bool overlap(const Rect& a, const Rect& b);
    inline bool overlap(const Circle& a, const Circle& b);
}
```
You can write automated unit tests that run in milliseconds in any CI pipeline without a GUI window!

---

## 2. The Universal Knob Drag State Machine

Knobs are the hardest physical control to model convincingly on a desktop computer. Users expect:
1. When clicking near the middle, dragging vertically up/down should turn the knob (standard in Ableton, Logic, and audio plugins).
2. When clicking on the outer edge, dragging circularly around the dial should track the finger/mouse angle.
3. When using a mouse wheel or trackpad, scrolling should step discrete clicks.
4. Small wiggles (< 4 pixels) should count as a click rather than a drag.

### State Transitions
```
[ Mouse Button Pressed ]
           |
    r < CENTER_ZONE_PX ?
    /                  \
 (YES)                 (NO)
   v                     v
[ Vertical Drag Mode ] [ Circular Arc Drag Mode ]
   |                     |
 delta_y / px_per_click  delta_angle / deg_per_click
   |                     |
   +-----------> [ Output: Detents / Continuous Degrees ]
```

---

## 3. OLED / Display Emulation Architecture

Real microcontrollers (RP2040, STM32, ESP32) write to monochrome OLEDs (like SSD1306) via page-buffered byte arrays:
* 128 columns $\times$ 64 rows = 8 pages of 8 vertical pixels (1024 bytes).

To emulate this:
1. Provide a lightweight `FrameBuffer` class with `draw_pixel(x, y, on)` and `draw_string(x, y, text)`.
2. In the Raylib drawing pass, loop over the columns and bitmasks, drawing filled rectangles at scale:
   ```cpp
   for (int page = 0; page < pages; ++page) {
       for (int x = 0; x < width; ++x) {
           uint8_t byte = buffer[x + page * width];
           if (!byte) continue;
           for (int bit = 0; bit < 8; ++bit) {
               if (byte & (1 << bit)) {
                   DrawRectangle(screen_x + x * scale,
                                 screen_y + (page * 8 + bit) * scale,
                                 scale - 1, scale - 1, oled_on_color);
               }
           }
       }
   }
   ```
This provides 100% faithful emulation of physical display refresh and rasterization.
