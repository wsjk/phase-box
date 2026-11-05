#include "raylib.h"
#include "PhaseBoxState.hpp"
#include "SimControls.hpp"
#include "PresetSession.hpp"
#include "TapTempo.hpp"
#include <cstdlib>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <iostream>

#include "CoreMidiBackend.hpp"   // macOS only (empty elsewhere)
#include "MidiChangeFilter.hpp"
#include "MidiRouter.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// The 128x64 OLED shows exactly what the active destination emits: waveform shape (P1), phase
// offset (P3), phase modulation (P6), clock multiplier (P10), Turing blend (P11), weight (P9).
void DrawOLEDDisplay(RenderTexture2D& oled_target, const PhaseBoxState& state, int active_dest,
                     const PresetSession& session, uint32_t now_ms) {
    BeginTextureMode(oled_target);
    ClearBackground(BLACK);

    DrawRectangleLines(0, 0, 128, 64, WHITE);
    DrawLine(0, 14, 128, 14, WHITE);
    DrawLine(0, 50, 128, 50, WHITE);

    std::string header = simui::page_name(state.active_page);
    header += " D" + std::to_string(active_dest + 1);
    DrawText(header.c_str(), 4, 3, 10, WHITE);
    // Preset slot, with '*' while there are edits that are not saved yet.
    std::string slot_tag = "S" + std::to_string(session.slot()) + (session.dirty() ? "*" : "");
    DrawText(slot_tag.c_str(), 104, 3, 10, WHITE);

    const int wave_width = 120;
    const int wave_height = 33;
    const int start_x = 4;
    const int start_y = 16;

    std::vector<float> curve = simui::preview_curve(state, active_dest, wave_width);
    std::vector<Vector2> points;
    points.reserve(curve.size());
    for (int x = 0; x < wave_width; ++x) {
        float py = start_y + wave_height - (curve[x] * wave_height);
        points.push_back({(float)(start_x + x), py});
    }
    for (size_t i = 1; i < points.size(); ++i) {
        DrawLineV(points[i-1], points[i], WHITE);
    }
    for (int mx : simui::preview_markers(state, active_dest, wave_width)) {
        mx = std::clamp(mx, 0, wave_width - 1);
        DrawCircle(start_x + mx, (int)points[mx].y, 2.0f, WHITE);
    }

    // Hold progress grows along the divider; a save/load message replaces the footer for 1.5 s.
    float hold = session.hold_progress(now_ms);
    if (hold > 0.0f) DrawRectangle(0, 49, (int)(128 * hold), 3, WHITE);
    const char* status = session.status(now_ms);
    std::string footer = status ? std::string(status) : simui::footer(state, state.active_page, active_dest);
    DrawText(footer.c_str(), 4, 53, 8, WHITE);

    EndTextureMode();
}

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

    const int screenWidth = 800;
    const int screenHeight = 680;
    InitWindow(screenWidth, screenHeight, "Phase Box - Polyrhythmic Macro Simulator");
    SetTargetFPS(60);

    PhaseBoxState state;
    state.turing.seed(static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
    int active_dest = 0;
    TapTempo tap_tempo;

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
    // MIDI goes straight to the hardware destination named by $PHASE_BOX_MIDI_DEST (default "FRASIL FM-1").
    // No virtual port is created and no MIDI Studio patching is needed. PHASE_BOX_MIDI_DEST=virtual brings
    // back the old "Phase Box Virtual Out" source.
    CoreMidiBackend midi_backend;
    midi_route::Router midi(midi_backend, midi_route::Config::from_env(std::getenv("PHASE_BOX_MIDI_DEST")));
    MidiChangeFilter midi_filter;           // like the device: send on change, plus a 1 s refresh
    uint32_t midi_epoch = midi.epoch();
    if (!midi_backend.ok()) std::printf("[midi] CoreMIDI client could not be created (status %d)\n", midi_backend.error());
#endif

    RenderTexture2D oled_target = LoadRenderTexture(128, 64);
    uint32_t last_cc_ms = 0;                 // MIDI goes out every 30 ms of frame time

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        clock_ms += dt * 1000.0;
        const uint32_t now_ms = static_cast<uint32_t>(clock_ms);

        if (IsKeyPressed(KEY_E)) state.active_page = simui::next_page(state.active_page, +1);
        if (IsKeyPressed(KEY_Q)) state.active_page = simui::next_page(state.active_page, -1);

        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))   simui::adjust(state, active_dest, state.active_page, +1);
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) simui::adjust(state, active_dest, state.active_page, -1);

        if (IsKeyPressed(KEY_M)) state.step_turing(); // same as the Mutate footswitch
        if (IsKeyPressed(KEY_R)) session.reset_to_defaults(state, now_ms);   // everything back to defaults
        if (IsKeyPressed(KEY_U)) session.undo_reset(state, now_ms);          // ...and back again (until the next edit)
        if (IsKeyPressed(KEY_LEFT_BRACKET))  session.select_slot(session.slot() - 1, state, now_ms);
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) session.select_slot(session.slot() + 1, state, now_ms);

        if (IsKeyPressed(KEY_T) || IsKeyPressed(KEY_SPACE)) {
            float tapped_bpm;
            if (tap_tempo.tap(now_ms, tapped_bpm)) state.osc.bpm = tapped_bpm;
            state.restart_phase();   // every tap restarts the LFOs at 0 degrees (Footswitch 1)
        }

        if (IsKeyPressed(KEY_ONE)) state.adc_expression = 0.0f;
        if (IsKeyPressed(KEY_TWO)) state.adc_expression = 0.5f;
        if (IsKeyPressed(KEY_THREE)) state.adc_expression = 1.0f;

        state.advance(dt); // master phase; Turing steps on each master cycle

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
        session.button(IsKeyDown(KEY_ENTER), state, now_ms);        // hold 1 s = save

        DrawOLEDDisplay(oled_target, state, active_dest, session, now_ms);

        BeginDrawing();
        ClearBackground(Color{ 20, 20, 25, 255 });

        DrawText("PHASE BOX - POLYRHYTHMIC MACRO SIMULATOR", 40, 20, 20, RAYWHITE);

        std::string macro_status = "Polyrhythm -> ";
        for (size_t i = 0; i < evaluated.size(); ++i) {
            macro_status += "[D" + std::to_string(i+1) + " CC" + std::to_string(evaluated[i].cc) + ":" + std::to_string(evaluated[i].value) + "] ";
        }
        DrawText(macro_status.c_str(), 40, 45, 12, GREEN);

        Rectangle sourceRec = { 0.0f, 0.0f, (float)oled_target.texture.width, (float)-oled_target.texture.height };
        Rectangle destRec = { (screenWidth - 640) / 2.0f, 75.0f, 640.0f, 320.0f };
        DrawTexturePro(oled_target.texture, sourceRec, destRec, { 0, 0 }, 0.0f, WHITE);
        DrawRectangleLines((int)destRec.x - 2, (int)destRec.y - 2, (int)destRec.width + 4, (int)destRec.height + 4, DARKGRAY);

        // Live destination table: everything the pages edit, and what each destination outputs now.
        DrawRectangle(40, 405, 720, 108, Color{ 30, 30, 38, 255 });
        DrawRectangleLines(40, 405, 720, 108, DARKGRAY);
        DrawText("DEST    CH    CC    MULT     WEIGHT    OFFSET    OUT", 55, 412, 12, ORANGE);
        for (size_t i = 0; i < state.macros.destinations.size() && i < evaluated.size(); ++i) {
            const MacroDestination& d = state.macros.destinations[i];
            Color c = ((int)i == active_dest) ? GREEN : LIGHTGRAY;
            DrawText(TextFormat("%sD%d     %2d    %3d    x%-6g  %+5.2f     %+5.2f     %3d",
                                ((int)i == active_dest) ? ">" : " ", (int)i + 1, d.midi_channel, d.cc_number,
                                d.clock_mult, d.scale, d.offset, evaluated[i].value),
                     45, 430 + (int)i * 17, 12, c);
        }
        DrawText(TextFormat("TURING  mix %.2f   mutation %.2f   value %.2f   ([M] steps it now)",
                            state.turing_mix, state.turing.mutation_prob, state.turing_norm), 55, 487, 12, SKYBLUE);

        DrawRectangle(40, 523, 720, 142, Color{ 30, 30, 38, 255 });
        DrawRectangleLines(40, 523, 720, 142, DARKGRAY);
        DrawText("CONTROLS & CONFIGURATION:", 55, 533, 12, ORANGE);
        DrawText(TextFormat("[E] Next Page | [Q] Prev Page   (now: %s, editing Dest %d)", simui::page_name(state.active_page), active_dest + 1), 55, 553, 12, LIGHTGRAY);
        DrawText("[W/S or Up/Down] Adjust value   (P4 CHAN, P5 CC, P9 WGT, P10 DIV edit the active dest; pick it on P8)", 55, 573, 12, LIGHTGRAY);
        DrawText("[T/Space] Tap  |  [1-3] Pedal 0/.5/1  |  [M] Step Turing  |  [R] Reset ALL  |  [U] Undo reset", 55, 593, 12, LIGHTGRAY);
        DrawText(TextFormat("[Enter] hold 1 s = SAVE  |  [ and ] = preset slot (now %d)  |  auto-saves 2 s after your last edit", session.slot()), 55, 613, 12, LIGHTGRAY);
        DrawText("P1 WAVE  P2 BPM  P3 PHASE  P4 CHAN  P5 CC  P6 PM  P7 MUTE  P8 DEST  P9 WGT  P10 DIV  P11 MIX", 55, 633, 10, GRAY);
#ifdef __APPLE__
        {
            std::string midi_line = "MIDI out: " + midi.status();
            if (midi_line.size() > 100) midi_line = midi_line.substr(0, 97) + "...";
            DrawText(midi_line.c_str(), 55, 649, 12, midi.connected() ? GREEN : YELLOW);
        }
#else
        DrawText("MIDI out: macOS only (this build runs without MIDI output)", 55, 649, 12, YELLOW);
#endif
        DrawText("[ESC] Quit", 680, 553, 12, RED);

        EndDrawing();
    }

    UnloadRenderTexture(oled_target);
    CloseWindow();
    return 0;
}
