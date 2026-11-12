// The simulator window: everything fits, nothing overlaps, and clicks land on the right control.
#include <gtest/gtest.h>
#include <vector>
#include "SimLayout.hpp"

using namespace simlayout;

namespace {
bool inside(const Layout& L, const Rect& r) { return r.x >= 0 && r.y >= 0 && r.x + r.w <= L.window_w && r.y + r.h <= L.window_h; }
bool inside(const Layout& L, const Circle& c) { return c.x - c.r >= 0 && c.y - c.r >= 0 && c.x + c.r <= L.window_w && c.y + c.r <= L.window_h; }
}

TEST(SimLayout, EverythingFitsInTheWindowForBothPanelSizes) {
    for (int rows : {32, 64}) {
        const Layout L = Layout::make(rows);
        EXPECT_TRUE(inside(L, L.enclosure)); EXPECT_TRUE(inside(L, L.oled_bezel)); EXPECT_TRUE(inside(L, L.oled_screen));
        EXPECT_TRUE(inside(L, L.knob_ring)); EXPECT_TRUE(inside(L, L.foot_tap)); EXPECT_TRUE(inside(L, L.foot_mutate));
        EXPECT_TRUE(inside(L, L.jack_midi)); EXPECT_TRUE(inside(L, L.jack_exp)); EXPECT_TRUE(inside(L, L.usb_port));
        EXPECT_TRUE(inside(L, L.treadle)); EXPECT_TRUE(inside(L, L.monitor));
        for (const Rect& b : L.buttons) EXPECT_TRUE(inside(L, b));
    }
}

TEST(SimLayout, TheDeviceControlsStayInsideTheEnclosureAndDoNotOverlap) {
    for (int rows : {32, 64}) {
        const Layout L = Layout::make(rows);
        const Rect& e = L.enclosure;
        auto in_enclosure = [&](const Circle& c) { return c.x - c.r >= e.x && c.x + c.r <= e.x + e.w && c.y - c.r >= e.y && c.y + c.r <= e.y + e.h; };
        for (const Circle* c : {&L.knob_ring, &L.foot_tap, &L.foot_mutate, &L.led_tap, &L.led_mutate, &L.led_power, &L.jack_midi, &L.jack_exp}) EXPECT_TRUE(in_enclosure(*c));
        EXPECT_TRUE(L.oled_bezel.x >= e.x && L.oled_bezel.x + L.oled_bezel.w <= e.x + e.w);
        EXPECT_TRUE(L.usb_port.x + L.usb_port.w <= e.x + e.w && L.usb_port.y >= e.y);

        const std::vector<Circle> circles = {L.knob_ring, L.foot_tap, L.foot_mutate, L.led_tap, L.led_mutate, L.led_power, L.jack_midi, L.jack_exp};
        for (size_t i = 0; i < circles.size(); ++i) {
            for (size_t j = i + 1; j < circles.size(); ++j) EXPECT_FALSE(overlap(circles[i], circles[j]));
            EXPECT_FALSE(overlap(circles[i], L.oled_bezel));
            EXPECT_FALSE(overlap(circles[i], L.usb_port));
        }
        EXPECT_FALSE(overlap(L.oled_bezel, L.usb_port));
    }
}

TEST(SimLayout, FollowsTheDrawingInTheUserGuide) {
    const Layout L = Layout::make(64);
    EXPECT_TRUE(L.oled_bezel.x + L.oled_bezel.w < L.knob.x - L.knob.r);        // OLED left of the encoder
    EXPECT_TRUE(L.foot_tap.y - L.foot_tap.r > L.oled_bezel.y + L.oled_bezel.h); // footswitches below the OLED
    EXPECT_TRUE(L.foot_tap.x < L.foot_mutate.x);                                // Tap on the left, Mutate on the right
    EXPECT_TRUE(L.jack_midi.x < L.jack_exp.x);                                  // MIDI OUT left, EXPRESSION IN right
    EXPECT_TRUE(L.jack_midi.y > L.foot_tap.y + L.foot_tap.r);                   // jacks along the bottom
    EXPECT_TRUE(L.usb_port.x > L.enclosure.x + L.enclosure.w / 2);              // USB-C at the top right
    EXPECT_TRUE(L.usb_port.y < L.enclosure.y + 40);
}

TEST(SimLayout, TheOledAndTheKnobShareACentreLineAtBothSizes) {
    for (int rows : {32, 64}) {
        const Layout L = Layout::make(rows);
        EXPECT_NEAR(L.oled_screen.y + L.oled_screen.h / 2, L.knob.y, 0.01f);
        EXPECT_NEAR(L.oled_screen.w, 128.0f * L.oled_scale, 0.01f);
        EXPECT_NEAR(L.oled_screen.h, static_cast<float>(rows * L.oled_scale), 0.01f);
    }
    EXPECT_EQ(Layout::make(48).oled_rows, 64);                                  // anything unknown falls back to 64
}

TEST(SimLayout, ClicksHitTheRightControl) {
    const Layout L = Layout::make(64);
    EXPECT_TRUE(hit_circle(L.knob, L.knob.x, L.knob.y));
    EXPECT_TRUE(hit_circle(L.knob, L.knob.x + L.knob.r - 1, L.knob.y));
    EXPECT_FALSE(hit_circle(L.knob, L.knob.x + L.knob.r + 2, L.knob.y));
    EXPECT_TRUE(hit_circle(L.knob, L.knob.x + L.knob.r + 10, L.knob.y, 20));    // the scroll area is a bit larger
    EXPECT_FALSE(hit_circle(L.knob, L.foot_mutate.x, L.foot_mutate.y));
    EXPECT_TRUE(hit_circle(L.foot_tap, L.foot_tap.x, L.foot_tap.y));
    EXPECT_FALSE(hit_circle(L.foot_tap, L.foot_mutate.x, L.foot_mutate.y));
    for (int i = 0; i < BUTTON_COUNT; ++i) {
        const Rect& b = L.buttons[i];
        EXPECT_TRUE(hit_rect(b, b.x + b.w / 2, b.y + b.h / 2));
        for (int j = 0; j < BUTTON_COUNT; ++j) if (j != i) EXPECT_FALSE(overlap(b, L.buttons[j]));
    }
    EXPECT_FALSE(hit_rect(L.buttons[0], L.buttons[1].x + 5, L.buttons[1].y + 5));
}

TEST(SimLayout, PedalPositionFromTheMouse) {
    const Layout L = Layout::make(64);
    EXPECT_NEAR(expression_from_y(L.treadle, L.treadle.y), 1.0f, 0.001f);                      // top = toe down
    EXPECT_NEAR(expression_from_y(L.treadle, L.treadle.y + L.treadle.h), 0.0f, 0.001f);        // bottom = heel down
    EXPECT_NEAR(expression_from_y(L.treadle, L.treadle.y + L.treadle.h / 2), 0.5f, 0.001f);
    EXPECT_NEAR(expression_from_y(L.treadle, -500), 1.0f, 0.001f);                              // dragging past the ends clamps
    EXPECT_NEAR(expression_from_y(L.treadle, 5000), 0.0f, 0.001f);
    EXPECT_NEAR(expression_from_y(L.treadle, y_from_expression(L.treadle, 0.3f)), 0.3f, 0.001f);
}

TEST(SimLayout, ScrollingTurnsTheKnobInWholeClicks) {
    float acc = 0;
    EXPECT_EQ(detents_from_wheel(acc, 1.0f), 2);              // one mouse-wheel notch
    EXPECT_EQ(detents_from_wheel(acc, -1.0f), -2);
    EXPECT_EQ(detents_from_wheel(acc, 0.2f), 0);              // a gentle trackpad scroll builds up...
    EXPECT_EQ(detents_from_wheel(acc, 0.2f), 0);
    EXPECT_EQ(detents_from_wheel(acc, 0.2f), 1);              // ...and clicks over
    EXPECT_NEAR(acc, 0.1f, 0.001f);                           // the remainder is kept
    float up = 0; int total = 0;
    for (int i = 0; i < 100; ++i) total += detents_from_wheel(up, 0.07f);
    EXPECT_NEAR(static_cast<float>(total), 14.0f, 1.0f);      // 7.0 of scroll = 14 clicks, none lost to rounding
}

// ---- dragging the knob ----
namespace {
int drag_circle(KnobDrag& d, const Circle& k, float radius, float from_deg, float to_deg, float step_deg) {
    int total = 0;
    const float dir = to_deg >= from_deg ? 1.0f : -1.0f;
    for (float a = from_deg + dir * step_deg; dir > 0 ? a <= to_deg + 0.01f : a >= to_deg - 0.01f; a += dir * step_deg)
        total += d.move(k.x + radius * std::cos(a * 3.14159265f / 180.0f), k.y + radius * std::sin(a * 3.14159265f / 180.0f));
    return total;
}
Circle K() { return Layout::make(64).knob; }
}

TEST(KnobDragTest, ASmallWiggleIsStillAClick) {
    const Circle k = K(); KnobDrag d;
    d.begin(k, k.x + 40, k.y);
    EXPECT_EQ(d.move(k.x + 42, k.y + 1), 0);
    EXPECT_EQ(d.move(k.x + 41, k.y - 2), 0);
    EXPECT_FALSE(d.dragged());
}

TEST(KnobDragTest, DraggingAroundTheKnobTurnsItAFifteenDegreeClick) {
    const Circle k = K(); KnobDrag d;
    d.begin(k, k.x + 40, k.y);                                           // grabbed at 3 o'clock
    const int cw = drag_circle(d, k, 40, 0, 90, 10);                      // clockwise quarter turn
    EXPECT_TRUE(d.dragged());
    EXPECT_GE(cw, 4); EXPECT_LE(cw, 6);                                   // about 80 degrees counted = 5 clicks, no jump at the start
    d.end();
    d.begin(k, k.x + 40, k.y);
    const int ccw = drag_circle(d, k, 40, 0, -90, 10);
    EXPECT_LE(ccw, -4); EXPECT_GE(ccw, -6);                               // anticlockwise gives negative clicks
}

TEST(KnobDragTest, AroundTheKnobWorksAcrossTheLeftSideWhereTheAngleWraps) {
    const Circle k = K(); KnobDrag d;
    d.begin(k, k.x - 40, k.y + 1);                                        // 9 o'clock, angle ~ 180 degrees
    int total = 0;
    for (int deg = 0; deg <= 90; deg += 10)                                // clockwise through the wrap point
        total += d.move(k.x + 40 * std::cos((180.0f + deg) * 3.14159265f / 180.0f), k.y + 40 * std::sin((180.0f + deg) * 3.14159265f / 180.0f));
    EXPECT_GE(total, 4); EXPECT_LE(total, 6);                             // still positive, no spurious huge jump
}

TEST(KnobDragTest, GrabbingTheMiddleDragsUpAndDown) {
    const Circle k = K(); KnobDrag d;
    d.begin(k, k.x + 3, k.y + 3);
    int total = 0;
    for (int dy = 1; dy <= 36; dy += 3) total += d.move(k.x + 3, k.y + 3 - dy);   // 36 px up
    EXPECT_GE(total, 4); EXPECT_LE(total, 6);                             // 6 px per click: about 5 after the first few px start the drag
    int down = 0;
    for (int dy = 1; dy <= 36; dy += 3) down += d.move(k.x + 3, k.y + 3 - 36 + dy);
    EXPECT_LE(down, -4);
}

TEST(KnobDragTest, NothingHappensWithoutAPressAndEndResetsIt) {
    const Circle k = K(); KnobDrag d;
    EXPECT_EQ(d.move(k.x + 50, k.y), 0);
    d.begin(k, k.x + 40, k.y);
    drag_circle(d, k, 40, 0, 60, 10);
    EXPECT_TRUE(d.dragged());
    d.end();
    EXPECT_FALSE(d.dragged());
    EXPECT_EQ(d.move(k.x, k.y - 40), 0);                                  // not active any more
}

TEST(KnobDragTest, MoveDegreesFollowsTheHandAndWrapsCleanly) {
    const Circle k = K(); KnobDrag d;
    d.begin(k, k.x + 40, k.y);
    float total = 0.0f;
    for (int deg = 10; deg <= 90; deg += 10)
        total += d.move_degrees(k.x + 40 * std::cos(deg * 3.14159265f / 180.0f), k.y + 40 * std::sin(deg * 3.14159265f / 180.0f));
    EXPECT_NEAR(total, 80.0f, 1.0f);                                      // the first step only starts the drag
    d.end(); d.begin(k, k.x - 40, k.y + 1);
    float across = 0.0f;
    for (int deg = 0; deg <= 90; deg += 10)
        across += d.move_degrees(k.x + 40 * std::cos((180.0f + deg) * 3.14159265f / 180.0f), k.y + 40 * std::sin((180.0f + deg) * 3.14159265f / 180.0f));
    EXPECT_NEAR(across, 80.0f, 2.0f);                                     // no jump at the +-180 wrap point
}

TEST(KnobDragTest, VerticalDragIsReportedInDegreesToo) {
    const Circle k = K(); KnobDrag d;
    d.begin(k, k.x + 2, k.y + 2);
    float total = 0.0f;
    for (int dy = 1; dy <= 30; dy += 3) total += d.move_degrees(k.x + 2, k.y + 2 - dy);
    EXPECT_NEAR(total, 21 * 2.5f, 3.0f);                                  // 2.5 degrees per pixel, counted from where the drag starts (7 px in)
}
