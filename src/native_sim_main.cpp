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

void DrawOLEDDisplay(RenderTexture2D& oled_target, const PhaseBoxState& state, int active_dest,
                     const PresetSession& session, uint32_t now_ms) {
    BeginTextureMode(oled_target);
    ClearBackground(BLACK);

    DrawRectangleLines(0, 0, 128, 64, WHITE);
    DrawLine(0, 14, 128, 14, WHITE);
    DrawLine(0, 50, 128, 50, WHITE);

    std::string header = simui::page_name(state.active_page);
    header += " D" + std::to_string(active_dest + 1);
    // Use default font for sharp pixel-art style inside the 128x64 OLED
    DrawText(header.c_str(), 4, 3, 10, WHITE);

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

    float hold = session.hold_progress(now_ms);
    if (hold > 0.0f) DrawRectangle(0, 49, (int)(128 * hold), 3, WHITE);
    const char* status = session.status(now_ms);
    std::string footer = status ? std::string(status) : simui::footer(state, state.active_page, active_dest);
    DrawText(footer.c_str(), 4, 53, 8, WHITE);

    EndTextureMode();
}

int main() {
    const int screenWidth = 860;
    const int screenHeight = 740;
    
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "Phase Box - Polyrhythmic Macro Simulator");
    SetTargetFPS(60);

    // Load larger Courier New font for UI and set bilinear filtering for smooth high-res scaling
    Font courierFont = LoadFontEx("/System/Library/Fonts/Supplemental/Courier New.ttf", 28, 0, 250);
    SetTextureFilter(courierFont.texture, TEXTURE_FILTER_BILINEAR);

    PhaseBoxState state;
    state.turing.seed(static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
    int active_dest = 0;
    TapTempo tap_tempo;

    static std::string preset_dir;
    const char* env_dir = std::getenv("PHASE_BOX_PRESET_DIR");
    preset_dir = (env_dir && *env_dir) ? env_dir : "./";
    if (preset_dir.back() != '/') preset_dir += '/';
    PresetManager::path_prefix = preset_dir.c_str();
    FilePresetStorage preset_storage;
    PresetSession session(preset_storage);
    double clock_ms = 0.0;
    session.load(state, 0);

#ifdef __APPLE__
    MIDIClientCreate(CFSTR("PhaseBox MIDI Generator"), NULL, NULL, &g_midiClient);
    MIDISourceCreate(g_midiClient, CFSTR("Phase Box Virtual Out"), &g_virtualOutput);
#endif

    RenderTexture2D oled_target = LoadRenderTexture(128, 64);
    // Set nearest-neighbor filtering on the OLED render texture so pixel art stays razor sharp
    SetTextureFilter(oled_target.texture, TEXTURE_FILTER_POINT);

    auto last_cc_time = std::chrono::steady_clock::now();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        clock_ms += dt * 1000.0;
        const uint32_t now_ms = static_cast<uint32_t>(clock_ms);

        if (IsKeyPressed(KEY_E)) state.active_page = simui::next_page(state.active_page, +1);
        if (IsKeyPressed(KEY_Q)) state.active_page = simui::next_page(state.active_page, -1);

        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))   simui::adjust(state, active_dest, state.active_page, +1);
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) simui::adjust(state, active_dest, state.active_page, -1);

        if (IsKeyPressed(KEY_M)) state.step_turing();
        if (IsKeyPressed(KEY_LEFT_BRACKET))  session.select_slot(session.slot() - 1, state, now_ms);
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) session.select_slot(session.slot() + 1, state, now_ms);

        if (IsKeyPressed(KEY_T) || IsKeyPressed(KEY_SPACE)) {
            float tapped_bpm;
            if (tap_tempo.tap(now_ms, tapped_bpm)) state.osc.bpm = tapped_bpm;
            state.restart_phase();
        }

        if (IsKeyPressed(KEY_ONE)) state.adc_expression = 0.0f;
        if (IsKeyPressed(KEY_TWO)) state.adc_expression = 0.5f;
        if (IsKeyPressed(KEY_THREE)) state.adc_expression = 1.0f;

        state.advance(dt);
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

        session.update(state, now_ms);
        session.button(IsKeyDown(KEY_ENTER), state, now_ms);

        DrawOLEDDisplay(oled_target, state, active_dest, session, now_ms);

        BeginDrawing();
        ClearBackground(Color{ 20, 20, 25, 255 });

        DrawTextEx(courierFont, "PHASE BOX - POLYRHYTHMIC MACRO SIMULATOR", (Vector2){40.0f, 20.0f}, 22.0f, 1.0f, RAYWHITE);

        std::string macro_status = "Polyrhythm -> ";
        for (size_t i = 0; i < evaluated.size(); ++i) {
            macro_status += "[D" + std::to_string(i+1) + " CC" + std::to_string(evaluated[i].cc) + ":" + std::to_string(evaluated[i].value) + "] ";
        }
        DrawTextEx(courierFont, macro_status.c_str(), (Vector2){40.0f, 50.0f}, 15.0f, 1.0f, GREEN);

        Rectangle sourceRec = { 0.0f, 0.0f, (float)oled_target.texture.width, (float)-oled_target.texture.height };
        Rectangle destRec = { (GetScreenWidth() - 680) / 2.0f, 82.0f, 680.0f, 330.0f };
        DrawTexturePro(oled_target.texture, sourceRec, destRec, { 0, 0 }, 0.0f, WHITE);
        DrawRectangleLines((int)destRec.x - 2, (int)destRec.y - 2, (int)destRec.width + 4, (int)destRec.height + 4, DARKGRAY);

        // Larger destination table section
        DrawRectangle(40, 424, 780, 128, Color{ 30, 30, 38, 255 });
        DrawRectangleLines(40, 424, 780, 128, DARKGRAY);
        DrawTextEx(courierFont, "DEST    CH    CC    MULT     WEIGHT    OFFSET    OUT", (Vector2){55.0f, 432.0f}, 15.0f, 1.0f, ORANGE);
        for (size_t i = 0; i < state.macros.destinations.size() && i < evaluated.size(); ++i) {
            const MacroDestination& d = state.macros.destinations[i];
            Color c = ((int)i == active_dest) ? GREEN : LIGHTGRAY;
            std::string line = TextFormat("%sD%d     %2d    %3d    x%-6g  %+5.2f     %+5.2f     %3d",
                                          ((int)i == active_dest) ? ">" : " ", (int)i + 1, d.midi_channel, d.cc_number,
                                          d.clock_mult, d.scale, d.offset, evaluated[i].value);
            DrawTextEx(courierFont, line.c_str(), (Vector2){45.0f, (float)(454 + (int)i * 21)}, 15.0f, 1.0f, c);
        }
        std::string turing_str = TextFormat("TURING  mix %.2f   mutation %.2f   value %.2f   ([M] steps it now)",
                                            state.turing_mix, state.turing.mutation_prob, state.turing_norm);
        DrawTextEx(courierFont, turing_str.c_str(), (Vector2){55.0f, 518.0f}, 14.0f, 1.0f, SKYBLUE);

        // Larger controls section
        DrawRectangle(40, 562, 780, 162, Color{ 30, 30, 38, 255 });
        DrawRectangleLines(40, 562, 780, 162, DARKGRAY);
        DrawTextEx(courierFont, "CONTROLS & CONFIGURATION:", (Vector2){55.0f, 570.0f}, 15.0f, 1.0f, ORANGE);
        
        std::string c1 = TextFormat("[E] Next Page | [Q] Prev Page   (now: %s, editing Dest %d)", simui::page_name(state.active_page), active_dest + 1);
        DrawTextEx(courierFont, c1.c_str(), (Vector2){55.0f, 594.0f}, 14.0f, 1.0f, LIGHTGRAY);
        DrawTextEx(courierFont, "[W/S or Up/Down] Adjust value   (P4 CHAN, P5 CC, P9 WGT, P10 DIV edit active dest; pick on P8)", (Vector2){55.0f, 616.0f}, 14.0f, 1.0f, LIGHTGRAY);
        DrawTextEx(courierFont, "[T / Space] Tap Tempo  |  [1-3] Expression 0 / 0.5 / 1  |  [M] Step Turing now", (Vector2){55.0f, 638.0f}, 14.0f, 1.0f, LIGHTGRAY);
        
        std::string c4 = TextFormat("[Enter] hold 1 s = SAVE  |  [ and ] = preset slot (now %d)  |  auto-saves after 2 s", session.slot());
        DrawTextEx(courierFont, c4.c_str(), (Vector2){55.0f, 660.0f}, 14.0f, 1.0f, LIGHTGRAY);
        DrawTextEx(courierFont, "P1 WAVE  P2 BPM  P3 PHASE  P4 CHAN  P5 CC  P6 PM  P7 MUTE  P8 DEST  P9 WGT  P10 DIV  P11 MIX", (Vector2){55.0f, 682.0f}, 12.0f, 1.0f, GRAY);
        
#ifdef __APPLE__
        DrawTextEx(courierFont, "MIDI out: \"Phase Box Virtual Out\" (CoreMIDI)", (Vector2){55.0f, 704.0f}, 13.0f, 1.0f, GREEN);
#else
        DrawTextEx(courierFont, "MIDI out: macOS only (this build runs without MIDI output)", (Vector2){55.0f, 704.0f}, 13.0f, 1.0f, YELLOW);
#endif
        DrawTextEx(courierFont, "[ESC] Quit", (Vector2){715.0f, 594.0f}, 14.0f, 1.0f, RED);

        EndDrawing();
    }

    UnloadFont(courierFont);
    UnloadRenderTexture(oled_target);
    CloseWindow();
    return 0;
}
