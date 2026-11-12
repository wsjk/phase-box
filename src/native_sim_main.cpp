#include "raylib.h"
#include "PhaseBoxState.hpp"
#include "SimControls.hpp"
#include "PresetSession.hpp"
#include "TapTempo.hpp"
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <iostream>

#include "CoreMidiBackend.hpp"   // macOS only (empty elsewhere)
#include "MidiChangeFilter.hpp"
#include "MidiRouter.hpp"
#include "OledRenderer.hpp"
#include "SimLayout.hpp"
#include "SimFont.hpp"
#include "SimPot.hpp"
#include "hal/frame_buffer.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Debug hook for the automated tests: receives every string the OLED draws. Unused in normal runs.
void (*g_sim_text_hook)(const char*) = nullptr;

namespace {

using simlayout::Circle;
using simlayout::Layout;

// ---- palette -------------------------------------------------------------------------------------
const Color kWindowBg   = {18, 19, 24, 255};
const Color kPanel      = {28, 30, 38, 255};
const Color kPanelEdge  = {55, 58, 70, 255};
const Color kBodyEdge   = {20, 22, 27, 255};
const Color kBody       = {46, 50, 61, 255};
const Color kBodyLight  = {58, 63, 76, 255};
const Color kSilk       = {214, 218, 226, 255};
const Color kSilkDim    = {150, 156, 168, 255};
const Color kMetal      = {170, 173, 182, 255};
const Color kMetalDark  = {96, 99, 109, 255};
const Color kShadow     = {12, 13, 16, 255};
const Color kAmber      = {255, 176, 46, 255};
const Color kCyan       = {96, 205, 255, 255};
const Color kGreenLed   = {90, 255, 130, 255};
const Color kOledBg     = {5, 9, 15, 255};
const Color kOledOn     = {172, 232, 255, 255};

Rectangle R(const simlayout::Rect& r) { return Rectangle{r.x, r.y, r.w, r.h}; }
Vector2 V(float x, float y) { return Vector2{x, y}; }
const float kPi = 3.14159265f;

// Records the strings the renderer draws (for the test hook) while drawing into the frame buffer.
class TextRecorder : public phasebox::hal::DisplayInterface {
public:
    explicit TextRecorder(phasebox::hal::FrameBuffer& fb) : fb_(fb) {}
    void clear() override { fb_.clear(); texts.clear(); }
    void draw_pixel(int16_t x, int16_t y, bool on) override { fb_.draw_pixel(x, y, on); }
    void draw_string(int16_t x, int16_t y, const char* str) override { fb_.draw_string(x, y, str); if (str) texts.emplace_back(str); }
    void update() override {}
    uint16_t get_width() const override { return fb_.get_width(); }
    uint16_t get_height() const override { return fb_.get_height(); }
    std::vector<std::string> texts;
private:
    phasebox::hal::FrameBuffer& fb_;
};

// Text: an embedded monospaced font (DejaVu Sans Mono), loaded at the screen's real resolution so it stays sharp on
// Retina displays. Sizes below are the nominal sizes used in the layout; kUiScale makes them comfortably readable.
Font g_font, g_font_bold;
bool g_font_custom = false;
const float kUiScale = 1.2f;

void load_fonts() {
    const float dpi = std::max(1.0f, GetWindowScaleDPI().x);
    const int base = static_cast<int>(15.0f * dpi + 0.5f);
    g_font = LoadFontFromMemory(".ttf", simfont::kRegular, static_cast<int>(sizeof(simfont::kRegular)), base, nullptr, 0);
    g_font_bold = LoadFontFromMemory(".ttf", simfont::kBold, static_cast<int>(sizeof(simfont::kBold)), base, nullptr, 0);
    g_font_custom = g_font.texture.id != 0 && g_font_bold.texture.id != 0;
    if (g_font_custom) {
        SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(g_font_bold.texture, TEXTURE_FILTER_BILINEAR);
    } else {                                   // could not load the embedded font: fall back to raylib's built-in one
        g_font = GetFontDefault();
        g_font_bold = g_font;
    }
}
void unload_fonts() { if (g_font_custom) { UnloadFont(g_font); UnloadFont(g_font_bold); } }

void ui_text(const char* text, float x, float y, float size, Color c, bool bold = false) {
    DrawTextEx(bold ? g_font_bold : g_font, text, V(x, y), size * kUiScale, 0.0f, c);
}
int ui_measure(const char* text, float size, bool bold = false) {
    return static_cast<int>(MeasureTextEx(bold ? g_font_bold : g_font, text, size * kUiScale, 0.0f).x);
}
void centered_text(const char* text, float cx, float y, int size, Color c, bool bold = false) {
    ui_text(text, cx - ui_measure(text, static_cast<float>(size), bold) / 2.0f, y, static_cast<float>(size), c, bold);
}

void draw_screw(float x, float y) {
    DrawCircle(static_cast<int>(x), static_cast<int>(y), 6, kShadow);
    DrawCircle(static_cast<int>(x), static_cast<int>(y), 4.5f, kMetal);
    DrawLineEx(V(x - 3, y + 1), V(x + 3, y - 1), 1.6f, kMetalDark);
}

void draw_led(const Circle& c, Color on_color, bool on) {
    const int x = static_cast<int>(c.x), y = static_cast<int>(c.y);
    if (on) {
        DrawCircle(x, y, c.r + 7, Fade(on_color, 0.20f));
        DrawCircle(x, y, c.r + 3.5f, Fade(on_color, 0.45f));
        DrawCircle(x, y, c.r, on_color);
        DrawCircle(x - 2, y - 2, 2, Fade(WHITE, 0.75f));
    } else {
        DrawCircle(x, y, c.r + 1.5f, kShadow);
        DrawCircle(x, y, c.r, Color{static_cast<unsigned char>(on_color.r / 6), static_cast<unsigned char>(on_color.g / 6), static_cast<unsigned char>(on_color.b / 6), 255});
    }
}

void draw_jack(const Circle& j) {
    const int x = static_cast<int>(j.x), y = static_cast<int>(j.y);
    DrawCircle(x, y, j.r + 5, kShadow);
    DrawCircle(x, y, j.r, kMetal);
    DrawCircle(x, y, j.r - 5, Color{8, 8, 10, 255});
    DrawCircle(x, y, j.r - 9, kMetalDark);
}

void draw_usb(const simlayout::Rect& r) {
    DrawRectangleRounded(Rectangle{r.x - 3, r.y - 3, r.w + 6, r.h + 6}, 0.5f, 8, kShadow);
    DrawRectangleRounded(R(r), 0.5f, 8, kMetal);
    DrawRectangleRounded(Rectangle{r.x + 3, r.y + 3, r.w - 6, r.h - 6}, 0.5f, 8, Color{8, 8, 10, 255});
    DrawRectangle(static_cast<int>(r.x + 10), static_cast<int>(r.y + r.h / 2 - 1), static_cast<int>(r.w - 20), 2, kMetalDark);
}

void draw_footswitch(const Circle& f, bool pressed) {
    const int x = static_cast<int>(f.x), y = static_cast<int>(f.y);
    DrawCircle(x, y, f.r + 6, kShadow);
    DrawCircle(x, y, f.r, kMetalDark);
    DrawCircle(x, y, f.r - 5, kMetal);
    const float cap = pressed ? f.r - 15 : f.r - 11;
    DrawCircle(x, y + (pressed ? 2 : 0), cap, Color{128, 131, 141, 255});
    for (int i = 1; i <= 4; ++i) DrawCircleLines(x, y + (pressed ? 2 : 0), cap * i / 5.0f, Fade(kShadow, 0.35f));   // brushed-metal rings
    if (!pressed) DrawCircle(x - static_cast<int>(f.r * 0.22f), y - static_cast<int>(f.r * 0.25f), f.r * 0.30f, Fade(WHITE, 0.12f));
}

void draw_knob(const Circle& k, float angle_deg, bool pressed, float hold_progress, bool pot) {
    const int x = static_cast<int>(k.x), y = static_cast<int>(k.y);
    DrawCircle(x, y, k.r + 16, kShadow);
    DrawCircle(x, y, k.r + 13, Color{36, 39, 48, 255});
    if (pot) {                                                         // pot: ticks only across the 270 degree sweep, end stops marked
        for (int i = 0; i <= 10; ++i) {
            const float deg = simpot::MIN_DEGREES + i * (simpot::SWEEP_DEGREES / 10.0f);
            const float a = (deg - 90.0f) * kPi / 180.0f;
            const bool stop = (i == 0 || i == 10);
            DrawLineEx(V(k.x + std::cos(a) * (k.r + 6), k.y + std::sin(a) * (k.r + 6)),
                       V(k.x + std::cos(a) * (k.r + (stop ? 15 : 11)), k.y + std::sin(a) * (k.r + (stop ? 15 : 11))), stop ? 3.0f : 2.0f, stop ? kAmber : kSilkDim);
        }
    } else {
        for (int i = 0; i < 24; ++i) {                                 // endless encoder: one tick per 15 degrees, like detents
            const float a = i * 15.0f * kPi / 180.0f;
            DrawLineEx(V(k.x + std::cos(a) * (k.r + 7), k.y + std::sin(a) * (k.r + 7)),
                       V(k.x + std::cos(a) * (k.r + 12), k.y + std::sin(a) * (k.r + 12)), 2.0f, kSilkDim);
        }
    }
    if (hold_progress > 0.0f) DrawRing(V(k.x, k.y), k.r + 14, k.r + 19, -90.0f, -90.0f + 360.0f * hold_progress, 64, kAmber);   // hold-to-save
    const float cap = pressed ? k.r - 3 : k.r;
    DrawCircle(x, y, cap + 3, kShadow);
    DrawCircle(x, y, cap, kMetalDark);
    DrawCircle(x, y, cap * 0.88f, kMetal);
    DrawCircle(x - static_cast<int>(cap * 0.22f), y - static_cast<int>(cap * 0.26f), cap * 0.50f, Fade(WHITE, 0.10f));
    for (int i = 1; i <= 3; ++i) DrawCircleLines(x, y, cap * 0.88f * i / 4.0f, Fade(kShadow, 0.25f));
    const float a = (angle_deg - 90.0f) * kPi / 180.0f;                // pointer
    DrawLineEx(V(k.x + std::cos(a) * cap * 0.30f, k.y + std::sin(a) * cap * 0.30f),
               V(k.x + std::cos(a) * cap * 0.80f, k.y + std::sin(a) * cap * 0.80f), 5.0f, Color{30, 32, 38, 255});
}

void draw_oled(const Layout& L, const phasebox::hal::FrameBuffer& fb) {
    DrawRectangleRounded(Rectangle{L.oled_bezel.x - 3, L.oled_bezel.y - 3, L.oled_bezel.w + 6, L.oled_bezel.h + 6}, 0.06f, 8, kShadow);
    DrawRectangleRounded(R(L.oled_bezel), 0.06f, 8, Color{12, 13, 17, 255});
    DrawRectangle(static_cast<int>(L.oled_screen.x), static_cast<int>(L.oled_screen.y), static_cast<int>(L.oled_screen.w), static_cast<int>(L.oled_screen.h), kOledBg);
    const int sc = L.oled_scale, w = fb.get_width(), pages = fb.get_height() / 8;
    const unsigned char* data = fb.data();
    for (int page = 0; page < pages; ++page)
        for (int x = 0; x < w; ++x) {
            const unsigned char column = data[x + page * w];
            if (!column) continue;
            for (int bit = 0; bit < 8; ++bit)
                if (column & (1 << bit))
                    DrawRectangle(static_cast<int>(L.oled_screen.x) + x * sc, static_cast<int>(L.oled_screen.y) + (page * 8 + bit) * sc, sc - 1, sc - 1, kOledOn);
        }
}

void draw_button(const simlayout::Rect& r, const char* label, bool active) {
    DrawRectangleRounded(R(r), 0.25f, 8, active ? Color{150, 100, 20, 255} : Color{40, 43, 54, 255});
    DrawRectangleRounded(Rectangle{r.x + 2, r.y + 2, r.w - 4, r.h - 4}, 0.25f, 8, active ? kAmber : Color{58, 62, 76, 255});
    centered_text(label, r.x + r.w / 2, r.y + r.h / 2 - 8, 12, active ? Color{30, 20, 5, 255} : kSilk, true);
}

void draw_treadle(const simlayout::Rect& t, float value) {
    DrawRectangleRounded(Rectangle{t.x - 4, t.y - 4, t.w + 8, t.h + 8}, 0.12f, 8, kShadow);
    DrawRectangleRounded(R(t), 0.12f, 8, Color{38, 41, 51, 255});
    const float hy = std::clamp(simlayout::y_from_expression(t, value), t.y + 16, t.y + t.h - 16);   // keep the handle on its track
    DrawRectangle(static_cast<int>(t.x + 8), static_cast<int>(hy), static_cast<int>(t.w - 16), static_cast<int>(t.y + t.h - hy - 8), Fade(kAmber, 0.35f));   // filled up to the pedal position
    DrawRectangleRounded(Rectangle{t.x + 4, hy - 13, t.w - 8, 26}, 0.35f, 8, kMetalDark);
    DrawRectangleRounded(Rectangle{t.x + 6, hy - 11, t.w - 12, 22}, 0.35f, 8, kMetal);
    centered_text(TextFormat("%.2f", value), t.x + t.w / 2, hy - 5, 10, Color{30, 32, 38, 255});   // the pedal position, on the handle
    centered_text("TOE", t.x + t.w / 2, t.y + 6, 10, kSilkDim);
    centered_text("HEEL", t.x + t.w / 2, t.y + t.h - 16, 10, kSilkDim);
}

} // namespace

int main(int argc, char** argv) {
    // `phase_box_native_sim --list-midi` prints what CoreMIDI reports (works even if Audio MIDI Setup is blank).
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--list-midi") {
#ifdef __APPLE__
            CoreMidiBackend backend;
            backend.print_all(stdout);
#else
            std::printf("MIDI output is macOS only.\n");
#endif
            return 0;
        }
    }

    // OLED rows: 64 by default; PHASE_BOX_OLED_ROWS=32 previews the 128x32 panel the firmware defaults to.
    const char* rows_env = std::getenv("PHASE_BOX_OLED_ROWS");
    const Layout L = Layout::make(rows_env ? std::atoi(rows_env) : 64);

    if (!std::getenv("PHASE_BOX_NO_HIGHDPI")) SetConfigFlags(FLAG_WINDOW_HIGHDPI);   // sharp on Retina screens
    InitWindow(L.window_w, L.window_h, "Phase Box - Polyrhythmic Macro Simulator");
    load_fonts();
    SetTargetFPS(60);

    PhaseBoxState state;
    state.turing.seed(static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
    int active_dest = 0;
    TapTempo tap_tempo;
    // The on-screen knob behaves like a pot (end stops, pointer shows the value). The unit itself has an endless encoder;
    // PHASE_BOX_KNOB=encoder gives the simulated knob that endless behaviour instead.
    const char* knob_env = std::getenv("PHASE_BOX_KNOB");
    const bool pot_mode = !(knob_env && std::string(knob_env) == "encoder");
    float pot_pos = 0.0f;                    // where the hand has taken the knob while dragging (0..1)

    // Presets are files in $PHASE_BOX_PRESET_DIR (default: the current directory): preset_1.bin .. preset_8.bin
    static std::string preset_dir;
    const char* env_dir = std::getenv("PHASE_BOX_PRESET_DIR");
    preset_dir = (env_dir && *env_dir) ? env_dir : "./";
    if (preset_dir.back() != '/') preset_dir += '/';
    PresetManager::path_prefix = preset_dir.c_str();
    FilePresetStorage preset_storage;
    PresetSession session(preset_storage);
    double clock_ms = 0.0;                   // frame-time clock: deterministic and wrap-free
    session.load(state, 0);                  // settings restore on power-up

#ifdef __APPLE__
    // MIDI goes straight to the hardware destination named by $PHASE_BOX_MIDI_DEST (default "Melodee FM-1").
    // No virtual port is created and no MIDI Studio patching is needed. PHASE_BOX_MIDI_DEST=virtual brings
    // back the old "Phase Box Virtual Out" source.
    CoreMidiBackend midi_backend;
    midi_route::Router midi(midi_backend, midi_route::Config::from_env(std::getenv("PHASE_BOX_MIDI_DEST")));
    MidiChangeFilter midi_filter;           // like the device: send on change, plus a 1 s refresh
    uint32_t midi_epoch = midi.epoch();
    if (!midi_backend.ok()) std::printf("[midi] CoreMIDI client could not be created (status %d)\n", midi_backend.error());
#endif

    phasebox::hal::FrameBuffer oled_fb(128, static_cast<uint16_t>(L.oled_rows));
    uint32_t last_cc_ms = 0;                 // MIDI goes out every 30 ms of frame time

    // mouse / knob state
    bool knob_down = false, tap_down = false, mutate_down = false, pedal_drag = false;
    float wheel_acc = 0.0f, knob_angle = 0.0f;
    simlayout::KnobDrag knob_drag;
    bool have_detent = false;
    uint32_t last_detent_ms = 0, mutate_flash_until = 0;
    float prev_turing_norm = state.turing_norm;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        clock_ms += dt * 1000.0;
        const uint32_t now_ms = static_cast<uint32_t>(clock_ms);

        auto turn = [&](int dir, bool fast) {            // one click: keys and scroll wheel
            if (pot_mode) { simpot::turn_clamped(state, active_dest, state.active_page, dir, fast); return; }   // a pot stops at the ends
            simui::adjust(state, active_dest, state.active_page, dir, fast);
            knob_angle += dir * 15.0f;
        };
        auto do_tap = [&]() {                            // Footswitch 1: tap tempo + restart the LFOs at 0 degrees
            float tapped_bpm;
            if (tap_tempo.tap(now_ms, tapped_bpm)) state.osc.bpm = tapped_bpm;
            state.restart_phase();
        };

        // ---- mouse: knob, footswitches, pedal, simulator buttons ----
        const Vector2 m = GetMousePosition();
        const bool lmb_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (simlayout::hit_circle(L.knob, m.x, m.y)) { knob_down = true; knob_drag.begin(L.knob, m.x, m.y); pot_pos = simpot::norm(state, active_dest, state.active_page); }   // encoder switch, or the start of a drag
            else if (simlayout::hit_circle(L.foot_tap, m.x, m.y)) { tap_down = true; do_tap(); }
            else if (simlayout::hit_circle(L.foot_mutate, m.x, m.y)) { mutate_down = true; state.step_turing(); }
            else if (simlayout::hit_rect(L.treadle, m.x, m.y, 6)) pedal_drag = true;
            else for (int i = 0; i < simlayout::BUTTON_COUNT; ++i) {
                if (!simlayout::hit_rect(L.buttons[i], m.x, m.y)) continue;
                switch (i) {
                    case simlayout::BTN_FREEZE: state.toggle_freeze(); break;
                    case simlayout::BTN_BYPASS: state.bypassed = !state.bypassed; break;
                    case simlayout::BTN_RESET: session.reset_to_defaults(state, now_ms); break;
                    case simlayout::BTN_UNDO: session.undo_reset(state, now_ms); break;
                    case simlayout::BTN_SLOT_PREV: session.select_slot(session.slot() - 1, state, now_ms); break;
                    case simlayout::BTN_SLOT_NEXT: session.select_slot(session.slot() + 1, state, now_ms); break;
                }
            }
        }
        if (!lmb_down) knob_down = tap_down = mutate_down = pedal_drag = false;
        if (knob_down) {                                  // dragging the knob turns it
            if (pot_mode) {                               // the pointer follows the hand, and stops at the end stops
                const float degrees = knob_drag.move_degrees(m.x, m.y);
                if (degrees != 0.0f) {
                    pot_pos = std::clamp(pot_pos + degrees / simpot::SWEEP_DEGREES, 0.0f, 1.0f);
                    simpot::set_norm(state, active_dest, state.active_page, pot_pos);
                }
            } else {
                const int clicks = knob_drag.move(m.x, m.y);
                for (int i = 0; i < std::abs(clicks); ++i) {
                    const bool fast = have_detent && static_cast<uint32_t>(now_ms - last_detent_ms) < 30u;
                    turn(clicks > 0 ? +1 : -1, fast);
                    have_detent = true; last_detent_ms = now_ms;
                }
            }
        }
        if (pedal_drag) state.adc_expression = simlayout::expression_from_y(L.treadle, m.y);

        const float wheel = GetMouseWheelMove();         // scroll over the knob turns it
        if (simlayout::hit_circle(L.knob_ring, m.x, m.y, 10)) {
            const int clicks = simlayout::detents_from_wheel(wheel_acc, wheel);
            for (int i = 0; i < std::abs(clicks); ++i) {
                const bool fast = have_detent && static_cast<uint32_t>(now_ms - last_detent_ms) < 30u;
                turn(clicks > 0 ? +1 : -1, fast);
                have_detent = true; last_detent_ms = now_ms;
            }
        } else {
            wheel_acc = 0.0f;
        }

        // ---- keyboard (everything still works) ----
        if (IsKeyPressed(KEY_E)) state.active_page = simui::next_page(state.active_page, +1);
        if (IsKeyPressed(KEY_Q)) state.active_page = simui::next_page(state.active_page, -1);

        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))   turn(+1, true);
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) turn(-1, true);

        if (IsKeyPressed(KEY_M)) state.step_turing(); // same as the Mutate footswitch
        if (IsKeyPressed(KEY_R)) session.reset_to_defaults(state, now_ms);   // everything back to defaults
        if (IsKeyPressed(KEY_F)) state.toggle_freeze();               // hold every output / let go again
        if (IsKeyPressed(KEY_B)) state.bypassed = !state.bypassed;    // every destination sends 0 / back to normal
        if (IsKeyPressed(KEY_U)) session.undo_reset(state, now_ms);          // ...and back again (until the next edit)
        if (IsKeyPressed(KEY_LEFT_BRACKET))  session.select_slot(session.slot() - 1, state, now_ms);
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) session.select_slot(session.slot() + 1, state, now_ms);

        if (IsKeyPressed(KEY_T) || IsKeyPressed(KEY_SPACE)) do_tap();

        if (IsKeyPressed(KEY_ONE)) state.adc_expression = 0.0f;
        if (IsKeyPressed(KEY_TWO)) state.adc_expression = 0.5f;
        if (IsKeyPressed(KEY_THREE)) state.adc_expression = 1.0f;

        state.advance(dt); // master phase; Turing steps on each master cycle
        if (state.turing_norm != prev_turing_norm) { prev_turing_norm = state.turing_norm; mutate_flash_until = now_ms + 150; }

#ifdef __APPLE__
        midi.poll(now_ms);
        if (midi.epoch() != midi_epoch) { midi_epoch = midi.epoch(); midi_filter = MidiChangeFilter(); }   // just connected: resend everything
        for (const std::string& line : midi.take_log()) std::printf("[midi] %s\n", line.c_str());
#endif

        auto evaluated = state.evaluateMacros(dt);

        if (static_cast<uint32_t>(now_ms - last_cc_ms) > 30) {
            #ifdef __APPLE__
            for (size_t i = 0; i < evaluated.size(); ++i) {
                const auto& msg = evaluated[i];
                if (midi_filter.should_send(i, static_cast<uint8_t>(msg.channel), static_cast<uint8_t>(msg.cc), static_cast<uint8_t>(msg.value), now_ms))
                    midi.send_cc(static_cast<uint8_t>(msg.channel), static_cast<uint8_t>(msg.cc), static_cast<uint8_t>(msg.value), now_ms);
            }
            #endif
            state.last_sent_cc_val = evaluated.empty() ? 0 : evaluated[0].value;
            last_cc_ms = now_ms;
        }

        session.update(state, now_ms);                              // auto-save 2 s after the last edit
        // Encoder switch (knob click or Enter): short press = next page, hold 1 s = save. Same as on the unit.
        const bool knob_click = knob_down && !knob_drag.dragged();   // a press that became a drag is not a click
        const auto switch_event = session.button(IsKeyDown(KEY_ENTER) || knob_click, state, now_ms);
        if (switch_event == PresetSession::Event::ShortPress && !knob_drag.dragged()) state.active_page = simui::next_page(state.active_page, +1);
        if (!lmb_down) knob_drag.end();

        // The OLED is drawn by the same renderer the firmware uses, then shown pixel by pixel.
        TextRecorder recorder(oled_fb);
        oled::draw(recorder, state, active_dest, session, now_ms);
        if (g_sim_text_hook) for (const std::string& t : recorder.texts) g_sim_text_hook(t.c_str());

        // ================================= drawing =================================
        BeginDrawing();
        ClearBackground(kWindowBg);

        // enclosure
        DrawRectangleRounded(Rectangle{L.enclosure.x - 4, L.enclosure.y - 4, L.enclosure.w + 8, L.enclosure.h + 8}, 0.05f, 16, kBodyEdge);
        DrawRectangleRounded(R(L.enclosure), 0.045f, 16, kBody);
        DrawRectangleRounded(Rectangle{L.enclosure.x + 6, L.enclosure.y + 6, L.enclosure.w - 12, 30}, 0.5f, 8, Fade(kBodyLight, 0.55f));   // top bevel highlight
        draw_screw(L.enclosure.x + 16, L.enclosure.y + 16);                       draw_screw(L.enclosure.x + L.enclosure.w - 16, L.enclosure.y + 16);
        draw_screw(L.enclosure.x + 16, L.enclosure.y + L.enclosure.h - 16);       draw_screw(L.enclosure.x + L.enclosure.w - 16, L.enclosure.y + L.enclosure.h - 16);
        ui_text("PHASE BOX", 60, 32, 26, kSilk, true);
        ui_text("POLYRHYTHMIC MACRO MODULATOR", 62, 66, 10, kSilkDim);
        draw_led(L.led_power, kGreenLed, true);

        draw_usb(L.usb_port);
        centered_text("USB-C", L.usb_port.x + L.usb_port.w / 2, L.usb_port.y + L.usb_port.h + 8, 10, kSilkDim);

        draw_oled(L, oled_fb);

        const float shown_angle = pot_mode ? simpot::MIN_DEGREES + simpot::SWEEP_DEGREES * simpot::norm(state, active_dest, state.active_page) : knob_angle;
        draw_knob(L.knob, shown_angle, knob_click, session.hold_progress(now_ms), pot_mode);
        if (pot_mode) {
            centered_text("MIN", L.knob.x - 86, L.knob.y + 62, 9, kAmber, true);
            centered_text("MAX", L.knob.x + 86, L.knob.y + 62, 9, kAmber, true);
        }
        centered_text("DRAG / SCROLL: VALUE", L.knob.x, 265, 9, kSilkDim);
        centered_text("CLICK: NEXT PAGE", L.knob.x, 279, 9, kSilkDim);
        centered_text("HOLD 1s: SAVE", L.knob.x, 293, 9, kSilkDim);

        draw_led(L.led_tap, kAmber, state.osc.phase < 0.8f);                       // flashes on every beat
        draw_led(L.led_mutate, kCyan, now_ms < mutate_flash_until);                // flashes on every Turing step
        draw_footswitch(L.foot_tap, tap_down);
        draw_footswitch(L.foot_mutate, mutate_down);
        centered_text("TAP / RESET", L.foot_tap.x, L.foot_tap.y + L.foot_tap.r + 5, 12, kSilk, true);
        centered_text("MUTATE / CYCLE", L.foot_mutate.x, L.foot_mutate.y + L.foot_mutate.r + 5, 12, kSilk, true);

        draw_jack(L.jack_midi);
        ui_text("MIDI OUT  DIN/TRS", static_cast<int>(L.jack_midi.x + 24), static_cast<int>(L.jack_midi.y - 8), 12, kSilk, true);
        draw_jack(L.jack_exp);
        const char* exp_label = "EXPRESSION IN  TRS";
        ui_text(exp_label, static_cast<int>(L.jack_exp.x - 24) - ui_measure(exp_label, 12, true), static_cast<int>(L.jack_exp.y - 8), 12, kSilk, true);
#ifdef __APPLE__
        draw_led(Circle{L.jack_midi.x + 24 + 138, L.jack_midi.y, 4}, kGreenLed, midi.connected());   // lit while the destination is connected
#endif

        // expression pedal (simulated) and its cable to the EXPRESSION IN jack
        const float cable_x = L.treadle.x + L.treadle.w / 2;
        DrawLineEx(V(L.jack_exp.x, L.jack_exp.y + 14), V(L.jack_exp.x, 505), 5.0f, Color{10, 10, 12, 255});
        DrawLineEx(V(L.jack_exp.x, 505), V(cable_x, 505), 5.0f, Color{10, 10, 12, 255});
        DrawLineEx(V(cable_x, 505), V(cable_x, L.treadle.y + L.treadle.h + 4), 5.0f, Color{10, 10, 12, 255});
        ui_text("EXP PEDAL", static_cast<int>(L.treadle.x - 4), 32, 12, kSilk, true);
        draw_treadle(L.treadle, state.adc_expression);

        // simulator-only shortcuts
        ui_text("SIMULATOR ONLY", static_cast<int>(L.buttons[0].x), 32, 12, kAmber, true);
        ui_text("(not on the unit)", static_cast<int>(L.buttons[0].x), 47, 9, kSilkDim);
        draw_button(L.buttons[simlayout::BTN_FREEZE], "FREEZE   (F)", state.frozen);
        draw_button(L.buttons[simlayout::BTN_BYPASS], "BYPASS   (B)", state.bypassed);
        draw_button(L.buttons[simlayout::BTN_RESET], "RESET ALL  (R)", false);
        draw_button(L.buttons[simlayout::BTN_UNDO], "UNDO RESET (U)", false);
        draw_button(L.buttons[simlayout::BTN_SLOT_PREV], "SLOT -     ( [ )", false);
        draw_button(L.buttons[simlayout::BTN_SLOT_NEXT], "SLOT +     ( ] )", false);

        // readouts
        const int mx = static_cast<int>(L.monitor.x), my = static_cast<int>(L.monitor.y);
        DrawRectangleRounded(R(L.monitor), 0.03f, 8, kPanel);
        std::string macro_status = "Polyrhythm -> ";
        for (size_t i = 0; i < evaluated.size(); ++i) {
            macro_status += "[D" + std::to_string(i+1) + " CC" + std::to_string(evaluated[i].cc) + ":" + std::to_string(evaluated[i].value) + "] ";
        }
        ui_text(macro_status.c_str(), mx + 15, my + 8, 12, GREEN);
        ui_text("DEST    CH    CC    MULT     WEIGHT    OFFSET    OUT", mx + 15, my + 28, 12, ORANGE);
        for (size_t i = 0; i < state.macros.destinations.size() && i < evaluated.size(); ++i) {
            const MacroDestination& d = state.macros.destinations[i];
            Color c = ((int)i == active_dest) ? GREEN : LIGHTGRAY;
            ui_text(TextFormat("%sD%d     %2d    %3d    x%-6g  %+5.2f     %+5.2f     %3d",
                                ((int)i == active_dest) ? ">" : " ", (int)i + 1, d.midi_channel, d.cc_number,
                                d.clock_mult, d.scale, d.offset, evaluated[i].value),
                     mx + 10, my + 46 + (int)i * 18, 12, c);
        }
        ui_text(TextFormat("TURING  mix %.2f   mutation %.2f   value %.2f   ([M] steps it now)",
                            state.turing_mix, state.turing.mutation_prob, state.turing_norm), mx + 15, my + 104, 12, SKYBLUE);
#ifdef __APPLE__
        {
            std::string midi_line = "MIDI out: " + midi.status();
            if (midi_line.size() > 100) midi_line = midi_line.substr(0, 97) + "...";
            ui_text(midi_line.c_str(), mx + 15, my + 124, 12, midi.connected() ? GREEN : YELLOW);
        }
#else
        ui_text("MIDI out: macOS only (this build runs without MIDI output)", mx + 15, my + 124, 12, YELLOW);
#endif
        ui_text(TextFormat("[E] Next Page | [Q] Prev Page   (now: %s, editing Dest %d)", simui::page_name(state.active_page), active_dest + 1), mx + 15, my + 148, 12, LIGHTGRAY);
        {   // which pages edit the selected destination (the list follows whatever pages exist)
            std::string dest_pages = "P4 CHAN, P5 CC, P9 WGT, P10 DIV";
            if (simui::PAGE_COUNT > 11) dest_pages += ", P12 OFF";
            ui_text(("[W/S or Up/Down] Adjust value   (" + dest_pages + " edit the active dest; pick it on P8)").c_str(), mx + 15, my + 166, 12, LIGHTGRAY);
        }
        ui_text("[T/Space] Tap  |  [1-3] Pedal 0/.5/1  |  [M] Step Turing  |  [R] Reset ALL  |  [U] Undo reset", mx + 15, my + 184, 12, LIGHTGRAY);
        ui_text(TextFormat("[Enter] hold = SAVE  |  [ ] = slot %d (auto-saves 2 s after an edit)  |  [F] FREEZE  |  [B] BYPASS (all 0)", session.slot()), mx + 15, my + 202, 12, LIGHTGRAY);
        ui_text("Mouse: drag or scroll the knob = turn  |  click knob = next page, hold 1 s = save  |  click footswitches  |  drag pedal", mx + 15, my + 220, 12, LIGHTGRAY);
        {   // every page, straight from the page table, so new pages show up here by themselves
            std::string pages;
            for (int i = 0; i < simui::PAGE_COUNT; ++i) {
                std::string n = simui::page_name(i);
                const size_t colon = n.find(": ");
                if (colon != std::string::npos) n.replace(colon, 2, " ");
                pages += n + "   ";
            }
            ui_text(pages.c_str(), mx + 15, my + 242, 10, GRAY);
        }
        ui_text("[ESC] Quit", mx + static_cast<int>(L.monitor.w) - 85, my + 8, 12, RED);

        EndDrawing();
    }

    unload_fonts();
    CloseWindow();
    return 0;
}
