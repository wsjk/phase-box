# Phase Box Simulator Stompbox Layout Reference

## Architecture Overview

The desktop simulator GUI is built on pure geometric primitives decoupled from Raylib drawing routines. This enables exhaustive unit testing of bounding boxes, overlaps, and hit-testing on any host platform without initializing a display server.

* **Layout Geometry:** `include/SimLayout.hpp`
* **Potentiometer Modeling & Detents:** `include/SimPot.hpp`
* **Bitmap Typography:** `include/SimFont.hpp`
* **Test Suites:** `tests/test_sim_layout.cpp` and `tests/test_sim_pot.cpp`

## Coordinate Space & Geometry (`simlayout::Layout`)

Default window dimensions: `1160 × 800` pixels.

```
(30, 20) +---------------------------------------------------------+
         | Enclosure (760 x 470)                                    |
         |                                                          |
         |  [OLED Bezel]                    (Knob / Ring)           |
         |  (70, cy-sh/2)                   (650, cy, r=58/72)      |
         |  cy = 190 px                     cy = 190 px             |
         |                                                          |
         |  [LED Tap]                       [LED Mutate]            |
         |                                                          |
         |  (Foot Tap)      (Expression)    (Foot Mutate)           |
         |  (262, 380, r=46) (456, 380,     (650, 380, r=46)        |
         |                   r=40/60)                               |
         +---------------------------------------------------------+
          (940, 64) [Simulator Action Buttons: 190 x 36 each]
```

### Alignment Constraints Verified in Tests
1. **Centerline Alignment:** The OLED display and the rotary encoder share the exact same vertical center line (`cy = 190.0f`).
2. **Non-Overlapping Controls:** Hit-test radiuses for both encoder knobs, footswitches, LEDs, and screen bezels never intersect.
3. **Responsive OLED Scaling:** Supports 32-row (128×32) and 64-row (128×64) panel rendering at 3× pixel scaling (`oled_scale = 3`).

## Knob Interaction Mechanics (`simlayout::KnobDrag`)

The encoder knob supports two drag modalities plus mouse wheel detents:

1. **Circular Drag Mode:**
   - Active when pressed outside the center zone ($r > 22\text{ px}$).
   - Tracks the angular arc of the cursor relative to the knob center $(x_c, y_c)$.
   - Every $15.0^\circ$ of angular rotation generates one detent step (`DEGREES_PER_CLICK`).
   - Cleanly handles wraparound across the $(-\pi, +\pi)$ boundary.

2. **Vertical Drag Mode:**
   - Active when pressed within the central zone ($r \le 22\text{ px}$).
   - Every $6.0\text{ px}$ of vertical travel generates one detent step (`PIXELS_PER_CLICK`).

3. **Mouse Wheel / Trackpad Scroll:**
   - Evaluated via `detents_from_wheel(float& acc, float wheel, float per_detent = 0.5f)`.
   - Accumulates wheel fractions and fires clean discrete clicks.

## Expression Encoder

* `L.exp_knob` / `L.exp_knob_ring`, between the footswitches. It uses its own `KnobDrag` and wheel accumulator, so it never turns the value knob.
* Every click calls `PhaseBoxState::turn_expression(±1)`: 1/20 per click, clamped to 0.0–1.0 (same code path as the firmware).
* `draw_expression_encoder()` shows the value as an amber arc from −135° (0.0) to +135° (1.0).
