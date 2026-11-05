#pragma once
// Draws the Phase Box screen onto any DisplayInterface (works for 128x32 and 128x64 panels):
//   page title + preset slot ("S1", "S1*" while unsaved)
//   ------------------------------------------------ (hold-to-save progress grows along this line)
//   the exact curve the active destination is sending, with a dot for the live position
//   ------------------------------------------------
//   value of the edited setting, or a short message (SAVED, EMPTY, NO STORAGE, ...)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "PhaseBoxState.hpp"
#include "PresetSession.hpp"
#include "SimControls.hpp"
#include "hal/hal_display.hpp"

namespace oled {

inline void line(phasebox::hal::DisplayInterface& d, int x0, int y0, int x1, int y1) {
    const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        d.draw_pixel(static_cast<int16_t>(x0), static_cast<int16_t>(y0), true);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

inline void draw(phasebox::hal::DisplayInterface& d, const PhaseBoxState& s, int active_dest,
                 const PresetSession& session, uint32_t now_ms) {
    const int W = d.get_width(), H = d.get_height();
    d.clear();

    char text[48];
    std::snprintf(text, sizeof text, "%s D%d", simui::page_name(s.active_page), active_dest + 1);
    d.draw_string(0, 0, text);
    char tag[8];
    std::snprintf(tag, sizeof tag, "S%d%s", session.slot(), session.dirty() ? "*" : "");
    d.draw_string(static_cast<int16_t>(W - 6 * static_cast<int>(std::strlen(tag))), 0, tag);

    const int top_line = 9, foot_line = H - 10;
    for (int x = 0; x < W; ++x) {
        d.draw_pixel(static_cast<int16_t>(x), static_cast<int16_t>(top_line), true);
        d.draw_pixel(static_cast<int16_t>(x), static_cast<int16_t>(foot_line), true);
    }
    const int hold_w = static_cast<int>(W * session.hold_progress(now_ms));
    for (int x = 0; x < hold_w; ++x) d.draw_pixel(static_cast<int16_t>(x), static_cast<int16_t>(top_line + 1), true);

    const int y_top = top_line + 2, y_bottom = foot_line - 2;
    const int curve_w = W - 4;
    const std::vector<float> curve = simui::preview_curve(s, active_dest, curve_w);
    auto y_of = [&](float v) { return y_bottom - static_cast<int>(std::lround(v * (y_bottom - y_top))); };
    for (int x = 1; x < curve_w; ++x) line(d, 2 + x - 1, y_of(curve[x - 1]), 2 + x, y_of(curve[x]));
    for (int mx : simui::preview_markers(s, active_dest, curve_w)) {
        mx = std::clamp(mx, 0, curve_w - 1);
        const int cy = y_of(curve[mx]);
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
                d.draw_pixel(static_cast<int16_t>(2 + mx + dx), static_cast<int16_t>(cy + dy), true);
    }

    if (const char* tag = simui::mode_tag(s)) {   // FROZEN / BYPASS in a box over the middle of the curve
        const int bw = static_cast<int>(std::strlen(tag)) * 6 + 6;
        const int bh = std::min(10, y_bottom - y_top + 1);
        const int bx = (W - bw) / 2, by = y_top + (y_bottom - y_top + 1 - bh) / 2;
        for (int y = by; y < by + bh; ++y)
            for (int x = bx; x < bx + bw; ++x)
                d.draw_pixel(static_cast<int16_t>(x), static_cast<int16_t>(y), x == bx || x == bx + bw - 1 || y == by || y == by + bh - 1);
        d.draw_string(static_cast<int16_t>(bx + 3), static_cast<int16_t>(by + (bh - 8) / 2), tag);
    }

    const char* status = session.status(now_ms);
    const std::string footer = status ? std::string(status) : simui::footer(s, s.active_page, active_dest);
    d.draw_string(0, static_cast<int16_t>(H - 8), footer.c_str());
}

} // namespace oled
