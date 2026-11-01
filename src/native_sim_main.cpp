#include "raylib.h"
#include "PhaseBoxState.hpp"
#include "SimControls.hpp"
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <iostream>

#ifdef __APPLE__
#include <CoreMIDI/CoreMIDI.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef __APPLE__
static MIDIClientRef g_midiClient = 0;
static MIDIEndpointRef g_virtualOutput = 0;

void SendMIDIControlChange(int channel, int control, int value) {
    if (g_virtualOutput == 0) return;

    Byte buffer[32] = {0};
    MIDIPacketList *packetList = (MIDIPacketList*)buffer;
    MIDIPacket *packet = MIDIPacketListInit(packetList);

    Byte midiMessage[3] = { (Byte)(0xB0 | ((channel - 1) & 0x0F)), (Byte)(control & 0x7F), (Byte)(value & 0x7F) };
    packet = MIDIPacketListAdd(packetList, sizeof(buffer), packet, 0, 3, midiMessage);

    MIDIReceived(g_virtualOutput, packetList);
}
#endif

// The 128x64 OLED shows exactly what the active destination emits: waveform shape (P1), phase
// offset (P3), phase modulation (P6), clock multiplier (P10), Turing blend (P11), weight (P9).
void DrawOLEDDisplay(RenderTexture2D& oled_target, const PhaseBoxState& state, int active_dest) {
    BeginTextureMode(oled_target);
    ClearBackground(BLACK);

    DrawRectangleLines(0, 0, 128, 64, WHITE);
    DrawLine(0, 14, 128, 14, WHITE);
    DrawLine(0, 50, 128, 50, WHITE);

    std::string header = simui::page_name(state.active_page);
    header += "  D" + std::to_string(active_dest + 1);
    DrawText(header.c_str(), 4, 3, 10, WHITE);

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

    std::string footer = simui::footer(state, state.active_page, active_dest);
    DrawText(footer.c_str(), 4, 53, 8, WHITE);

    EndTextureMode();
}

int main() {
    const int screenWidth = 800;
    const int screenHeight = 680;
    InitWindow(screenWidth, screenHeight, "Phase Box - Polyrhythmic Macro Simulator");
    SetTargetFPS(60);

    PhaseBoxState state;
    state.turing.seed(static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
    int active_dest = 0;
    std::vector<std::chrono::steady_clock::time_point> tap_times;

#ifdef __APPLE__
    MIDIClientCreate(CFSTR("PhaseBox MIDI Generator"), NULL, NULL, &g_midiClient);
    MIDISourceCreate(g_midiClient, CFSTR("Phase Box Virtual Out"), &g_virtualOutput);
#endif

    RenderTexture2D oled_target = LoadRenderTexture(128, 64);
    auto last_cc_time = std::chrono::steady_clock::now();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_E)) state.active_page = simui::next_page(state.active_page, +1);
        if (IsKeyPressed(KEY_Q)) state.active_page = simui::next_page(state.active_page, -1);

        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))   simui::adjust(state, active_dest, state.active_page, +1);
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) simui::adjust(state, active_dest, state.active_page, -1);

        if (IsKeyPressed(KEY_M)) state.step_turing(); // same as the Mutate footswitch

        if (IsKeyPressed(KEY_T) || IsKeyPressed(KEY_SPACE)) {
            auto now = std::chrono::steady_clock::now();
            if (!tap_times.empty() && std::chrono::duration_cast<std::chrono::milliseconds>(now - tap_times.back()).count() > 2500) {
                tap_times.clear();
            }
            tap_times.push_back(now);
            if (tap_times.size() > 4) tap_times.erase(tap_times.begin());
            if (tap_times.size() >= 2) {
                double total_ms = 0;
                for (size_t i = 1; i < tap_times.size(); ++i) {
                    total_ms += std::chrono::duration_cast<std::chrono::milliseconds>(tap_times[i] - tap_times[i-1]).count();
                }
                double avg_ms = total_ms / (tap_times.size() - 1);
                if (avg_ms > 0) {
                    state.osc.bpm = static_cast<float>(60000.0 / avg_ms);
                    state.osc.bpm = std::clamp(state.osc.bpm, 40.0f, 300.0f);
                }
            }
        }

        if (IsKeyPressed(KEY_ONE)) state.adc_expression = 0.0f;
        if (IsKeyPressed(KEY_TWO)) state.adc_expression = 0.5f;
        if (IsKeyPressed(KEY_THREE)) state.adc_expression = 1.0f;

        state.advance(dt); // master phase; Turing steps on each master cycle

        auto evaluated = state.evaluateMacros(dt);

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_cc_time).count() > 30) {
            #ifdef __APPLE__
            for (const auto& msg : evaluated) {
                SendMIDIControlChange(msg.channel, msg.cc, msg.value);
            }
            #endif
            state.last_sent_cc_val = evaluated.empty() ? 0 : evaluated[0].value;
            last_cc_time = now;
        }

        DrawOLEDDisplay(oled_target, state, active_dest);

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
        DrawText("[T / Space] Tap Tempo  |  [1-3] Expression 0 / 0.5 / 1  |  [M] Step Turing now", 55, 593, 12, LIGHTGRAY);
        DrawText("P1 WAVE  P2 BPM  P3 PHASE  P4 CHAN  P5 CC  P6 PM  P7 MUTE  P8 DEST  P9 WGT  P10 DIV  P11 MIX", 55, 613, 10, GRAY);
#ifdef __APPLE__
        DrawText("MIDI out: \"Phase Box Virtual Out\" (CoreMIDI)", 55, 635, 12, GREEN);
#else
        DrawText("MIDI out: macOS only (this build runs without MIDI output)", 55, 635, 12, YELLOW);
#endif
        DrawText("[ESC] Quit", 680, 553, 12, RED);

        EndDrawing();
    }

    UnloadRenderTexture(oled_target);
    CloseWindow();
    return 0;
}
