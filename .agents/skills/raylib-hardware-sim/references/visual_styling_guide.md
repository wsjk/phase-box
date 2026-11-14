# Hardware Visual Styling & Skeuomorphism in Raylib

To make a desktop simulator look like a real physical stompbox or hardware chassis rather than a primitive flat wireframe, use layered 2D rendering primitives:

---

## 1. Stompbox Enclosure Depth & Bevels

Physical enclosures have rounded aluminum corners, cast shadows, and subtle specular edge bevels:

1. **Drop Shadow (Bottom Layer):**
   ```cpp
   DrawRectangleRounded(Rectangle{box.x - 4, box.y - 4, box.w + 8, box.h + 8}, 0.05f, 16, Color{15, 17, 22, 255});
   ```
2. **Main Metal Body (Middle Layer):**
   ```cpp
   DrawRectangleRounded(box, 0.045f, 16, Color{46, 50, 61, 255});
   ```
3. **Top Bevel Highlight (Top Layer):**
   Draw a very slim, semi-transparent white/light rectangle along the top edge:
   ```cpp
   DrawRectangleRounded(Rectangle{box.x + 6, box.y + 6, box.w - 12, 25}, 0.5f, 8, Fade(WHITE, 0.08f));
   ```

---

## 2. Corner Screws

Cast-aluminum stompboxes have 4 corner screws:
```cpp
void draw_screw(float x, float y) {
    DrawCircle(x, y, 6, Color{12, 13, 16, 255});       // Deep shadow hole
    DrawCircle(x, y, 4.5f, Color{170, 173, 182, 255}); // Nickel-plated screw head
    DrawLineEx(Vector2{x - 3, y + 1}, Vector2{x + 3, y - 1}, 1.5f, Color{70, 72, 80, 255}); // Slotted head
}
```

---

## 3. Physical LEDs with Glow Halos

An illuminated LED casts an optical glow onto the surrounding chassis:

```cpp
void draw_led(float cx, float cy, float radius, Color color, bool on) {
    if (on) {
        // Outer diffuse halo
        DrawCircle(cx, cy, radius + 7, Fade(color, 0.20f));
        // Mid-intensity halo
        DrawCircle(cx, cy, radius + 3.5f, Fade(color, 0.45f));
        // Core bright diode
        DrawCircle(cx, cy, radius, color);
        // Specular reflection highlight on diode dome
        DrawCircle(cx - 2, cy - 2, 2, Fade(WHITE, 0.75f));
    } else {
        // Unlit / dark bezel
        DrawCircle(cx, cy, radius + 1.5f, Color{15, 17, 22, 255});
        DrawCircle(cx, cy, radius, Color{static_cast<unsigned char>(color.r / 6),
                                         static_cast<unsigned char>(color.g / 6),
                                         static_cast<unsigned char>(color.b / 6), 255});
    }
}
```

---

## 4. Heavy-Duty Footswitches

Physical 3PDT / Carling stomp switches consist of:
1. Outer mounting nut / washer.
2. Nickel-plated cylinder body.
3. Brushed concentric rings on the plunger cap (`DrawCircleLines`).
4. Mechanical depression on click (decrease cap radius by 4px and shift Y down 2px).

```cpp
void draw_footswitch(float cx, float cy, float radius, bool pressed) {
    DrawCircle(cx, cy, radius + 6, Color{15, 17, 22, 255});       // Shadow
    DrawCircle(cx, cy, radius, Color{90, 93, 103, 255});          // Nut
    DrawCircle(cx, cy, radius - 5, Color{170, 173, 182, 255});    // Bezel
    
    float cap_radius = pressed ? radius - 15 : radius - 11;
    float offset_y = pressed ? 2.0f : 0.0f;
    DrawCircle(cx, cy + offset_y, cap_radius, Color{130, 133, 143, 255}); // Plunger
    
    // Machined concentric grooves
    for (int i = 1; i <= 4; ++i) {
        DrawCircleLines(cx, cy + offset_y, cap_radius * i / 5.0f, Fade(Color{20, 20, 25, 255}, 0.35f));
    }
}
```
