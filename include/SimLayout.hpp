#pragma once
// Where everything sits in the simulator window. The enclosure follows the drawing in docs/USER_GUIDE.md
// (section 1): OLED on the left, rotary encoder on the right, two footswitches below, MIDI OUT and
// EXPRESSION IN jacks along the bottom, USB-C top right. Pure geometry (no graphics library), so it is
// unit-tested: bounds, overlaps, hit-testing, mouse-wheel and pedal mapping.
#include <algorithm>
#include <cmath>

namespace simlayout {

struct Rect { float x = 0, y = 0, w = 0, h = 0; };
struct Circle { float x = 0, y = 0, r = 0; };

inline bool hit_rect(const Rect& r, float px, float py, float margin = 0) {
    return px >= r.x - margin && px <= r.x + r.w + margin && py >= r.y - margin && py <= r.y + r.h + margin;
}
inline bool hit_circle(const Circle& c, float px, float py, float margin = 0) {
    const float dx = px - c.x, dy = py - c.y, rr = c.r + margin;
    return dx * dx + dy * dy <= rr * rr;
}
inline bool overlap(const Rect& a, const Rect& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}
inline bool overlap(const Circle& a, const Circle& b) {
    const float dx = a.x - b.x, dy = a.y - b.y, rr = a.r + b.r;
    return dx * dx + dy * dy < rr * rr;
}
inline bool overlap(const Circle& c, const Rect& r) {
    const float nx = std::clamp(c.x, r.x, r.x + r.w), ny = std::clamp(c.y, r.y, r.y + r.h);
    const float dx = c.x - nx, dy = c.y - ny;
    return dx * dx + dy * dy < c.r * c.r;
}

enum Button { BTN_FREEZE, BTN_BYPASS, BTN_RESET, BTN_UNDO, BTN_SLOT_PREV, BTN_SLOT_NEXT, BUTTON_COUNT };

struct Layout {
    int window_w = 1160, window_h = 800;
    int oled_rows = 64;
    int oled_scale = 3;                       // screen pixels per OLED pixel
    Rect enclosure, oled_bezel, oled_screen;
    Circle knob, knob_ring;                   // encoder cap, and the ring around it (turn / hold feedback)
    Circle foot_tap, foot_mutate;
    Circle led_tap, led_mutate, led_power;
    Circle jack_midi, jack_exp;
    Rect usb_port;
    Rect treadle;                             // the simulated expression pedal (drag to move)
    Rect buttons[BUTTON_COUNT];               // simulator-only shortcuts (not on the real unit)
    Rect monitor;                             // text readouts under the enclosure

    static Layout make(int rows) {
        Layout L;
        L.oled_rows = (rows == 32) ? 32 : 64;
        L.enclosure = {30, 20, 760, 470};
        const float cy = 190;                                   // the OLED and the knob share this centre line
        const float sw = 128.0f * L.oled_scale, sh = static_cast<float>(L.oled_rows * L.oled_scale);
        L.oled_screen = {70, cy - sh / 2, sw, sh};
        L.oled_bezel = {L.oled_screen.x - 10, L.oled_screen.y - 10, sw + 20, sh + 20};
        L.knob = {650, cy, 58};
        L.knob_ring = {650, cy, 72};
        L.foot_tap = {262, 380, 46};
        L.foot_mutate = {650, 380, 46};
        L.led_tap = {262, 320, 7};
        L.led_mutate = {650, 320, 7};
        L.led_power = {270, 50, 4};
        L.jack_midi = {110, 468, 14};
        L.jack_exp = {690, 468, 14};
        L.usb_port = {702, 36, 52, 20};
        L.treadle = {830, 60, 80, 300};
        for (int i = 0; i < BUTTON_COUNT; ++i) L.buttons[i] = {940, 64.0f + 48.0f * i, 190, 36};
        L.monitor = {30, 520, 1100, 260};
        return L;
    }
};

/// Pedal position from the mouse height: top of the treadle = toe down = 1.0, bottom = heel down = 0.0.
inline float expression_from_y(const Rect& treadle, float y) {
    return std::clamp(1.0f - (y - treadle.y) / treadle.h, 0.0f, 1.0f);
}
inline float y_from_expression(const Rect& treadle, float e) {
    return treadle.y + (1.0f - std::clamp(e, 0.0f, 1.0f)) * treadle.h;
}

/// Mouse wheel / trackpad scroll -> whole encoder clicks. Fractions are carried over in `acc`.
inline int detents_from_wheel(float& acc, float wheel, float per_detent = 0.5f) {
    acc += wheel;
    const int n = static_cast<int>(acc / per_detent);
    acc -= n * per_detent;
    return n;
}

/// Turning the on-screen knob with the mouse. Press on it, then drag: around the knob (clockwise = up) or, if you
/// grabbed it near the middle, straight up/down. Small movements are still a click (page change / hold to save).
class KnobDrag {
public:
    static constexpr float DEGREES_PER_CLICK = 15.0f;
    static constexpr float PIXELS_PER_CLICK = 6.0f;   // vertical mode
    static constexpr float DRAG_START_PX = 4.0f;      // less movement than this is still a click
    static constexpr float CENTER_ZONE_PX = 22.0f;    // pressed this close to the centre = vertical mode

    void begin(const Circle& knob, float x, float y) {
        cx_ = knob.x; cy_ = knob.y; px_ = x; py_ = y;
        angular_ = std::hypot(x - cx_, y - cy_) > CENTER_ZONE_PX;
        dragged_ = false; acc_ = 0.0f; last_angle_ = angle_at(x, y); last_y_ = y; active_ = true;
    }

    /// Call with the mouse position every frame while the button is down. Returns whole clicks (+ = clockwise/up).
    int move(float x, float y) {
        acc_ += step_degrees(x, y);
        const int clicks = static_cast<int>(acc_ / DEGREES_PER_CLICK);
        acc_ -= clicks * DEGREES_PER_CLICK;
        return clicks;
    }

    /// Same drag, as degrees turned since the last call (+ = clockwise/up): for a knob that follows the hand.
    float move_degrees(float x, float y) { return step_degrees(x, y); }

    bool dragged() const { return dragged_; }   // true once the press turned into a drag
    void end() { active_ = false; dragged_ = false; acc_ = 0.0f; }

private:
    float step_degrees(float x, float y) {
        if (!active_) return 0.0f;
        if (!dragged_) {
            if (std::hypot(x - px_, y - py_) <= DRAG_START_PX) return 0.0f;
            dragged_ = true; last_angle_ = angle_at(x, y); last_y_ = y; acc_ = 0.0f;   // count from here, no jump
            return 0.0f;
        }
        if (angular_) {
            const float a = angle_at(x, y);
            float d = a - last_angle_;
            while (d > 180.0f) d -= 360.0f;
            while (d < -180.0f) d += 360.0f;
            last_angle_ = a;
            return d;
        }
        const float d = (last_y_ - y) * (DEGREES_PER_CLICK / PIXELS_PER_CLICK);
        last_y_ = y;
        return d;
    }
    float angle_at(float x, float y) const { return std::atan2(y - cy_, x - cx_) * 180.0f / 3.14159265f; }
    float cx_ = 0, cy_ = 0, px_ = 0, py_ = 0, acc_ = 0, last_angle_ = 0, last_y_ = 0;
    bool angular_ = true, dragged_ = false, active_ = false;
};

} // namespace simlayout
