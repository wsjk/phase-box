#include "raylib.h"
#include <cmath>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>

// Simulated Hardware State with Menu Pages
struct PhaseBoxState {
    int current_wave = 0;       // 0: Sine, 1: Saw, 2: Square, 3: Triangle
    int bpm = 120;              // Tap Tempo BPM
    float phase_offset = 0.0f;  // Phase offset value
    int active_page = 0;        // 0: Waveform, 1: BPM, 2: Phase Offset
    bool button_state = false;  // Encoder Push Button State
    float adc_expression = 0.0f;// Expression Pedal ADC input (0.0 to 1.0)
};

// Draw 128x64 OLED Virtual Buffer with Menu Pages
void DrawOLEDDisplay(RenderTexture2D& oled_target, const PhaseBoxState& state, float phase_anim) {
    BeginTextureMode(oled_target);
    ClearBackground(BLACK);

    // Draw OLED Border & UI Layout (128x64 native resolution)
    DrawRectangleLines(0, 0, 128, 64, WHITE);
    DrawLine(0, 14, 128, 14, WHITE);
    DrawLine(0, 50, 128, 50, WHITE);

    // Header: Show current active menu page
    const char* page_names[] = {"P1: WAVE", "P2: BPM", "P3: PHASE"};
    std::string header = std::string(page_names[state.active_page]) + (state.button_state ? "*" : "");
    DrawText(header.c_str(), 4, 3, 10, WHITE);

    // Waveform Viewport (Rows 15 to 49)
    const int wave_width = 120;
    const int wave_height = 33;
    const int start_x = 4;
    const int start_y = 16;

    std::vector<Vector2> points;
    for (int x = 0; x < wave_width; ++x) {
        float t = (static_cast<float>(x) / wave_width) * 2.0f * 3.14159265f + phase_anim + state.phase_offset;
        float val = 0.0f;
        switch (state.current_wave % 4) {
            case 0: val = std::sin(t); break;
            case 1: val = 1.0f - std::fmod(t / 3.14159265f, 2.0f); break;
            case 2: val = (std::sin(t) >= 0.0f) ? 1.0f : -1.0f; break;
            case 3: val = 2.0f * std::abs(2.0f * (t / (2.0f * 3.14159265f) - std::floor(t / (2.0f * 3.14159265f) + 0.5f))) - 1.0f; break;
        }
        val *= (0.5f + 0.5f * state.adc_expression);

        float py = start_y + (wave_height / 2.0f) - (val * (wave_height / 2.0f - 2));
        points.push_back({(float)(start_x + x), py});
    }

    for (size_t i = 1; i < points.size(); ++i) {
        DrawLineV(points[i-1], points[i], WHITE);
    }

    // Footer: Show parameter value for the active page
    std::string footer = "";
    const char* wave_names[] = {"SINE", "SAW", "SQR", "TRI"};
    if (state.active_page == 0) {
        footer = "VAL: " + std::string(wave_names[state.current_wave % 4]);
    } else if (state.active_page == 1) {
        footer = "VAL: " + std::to_string(state.bpm) + " BPM";
    } else {
        footer = "VAL: " + std::string(TextFormat("%.2f", state.phase_offset));
    }
    DrawText(footer.c_str(), 4, 53, 8, WHITE);

    EndTextureMode();
}

int main() {
    const int screenWidth = 800;
    const int screenHeight = 540;
    InitWindow(screenWidth, screenHeight, "Phase Box - Native Hardware Simulator");
    SetTargetFPS(60);

    RenderTexture2D oled_target = LoadRenderTexture(128, 64);

    PhaseBoxState state;
    float phase_anim = 0.0f;
    std::vector<std::chrono::steady_clock::time_point> tap_times;

    while (!WindowShouldClose()) {
        // --- INPUT HANDLING ---
        if (IsKeyPressed(KEY_E)) {
            state.button_state = !state.button_state;
            if (state.button_state) {
                state.active_page = (state.active_page + 1) % 3;
            }
        }

        bool inc = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP);
        bool dec = IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN);

        if (inc) {
            if (state.active_page == 0) state.current_wave = (state.current_wave + 1) % 4;
            else if (state.active_page == 1) state.bpm = std::min(300, state.bpm + 5);
            else if (state.active_page == 2) state.phase_offset += 0.2f;
        }
        if (dec) {
            if (state.active_page == 0) state.current_wave = (state.current_wave + 3) % 4;
            else if (state.active_page == 1) state.bpm = std::max(40, state.bpm - 5);
            else if (state.active_page == 2) state.phase_offset -= 0.2f;
        }

        if (IsKeyPressed(KEY_ONE)) state.adc_expression = 0.0f;
        if (IsKeyPressed(KEY_TWO)) state.adc_expression = 0.5f;
        if (IsKeyPressed(KEY_THREE)) state.adc_expression = 1.0f;

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
                    state.bpm = static_cast<int>(60000.0 / avg_ms);
                    state.bpm = std::clamp(state.bpm, 40, 300);
                }
            }
        }

        float dt = GetFrameTime();
        phase_anim += (state.bpm / 120.0f) * dt * 3.0f;

        DrawOLEDDisplay(oled_target, state, phase_anim);

        BeginDrawing();
        ClearBackground(Color{ 20, 20, 25, 255 });

        DrawText("PHASE BOX EMBEDDED SIMULATOR", 40, 25, 20, RAYWHITE);
        DrawText("Hardware Benchtop & OLED Monitor", 40, 50, 14, GRAY);

        Rectangle sourceRec = { 0.0f, 0.0f, (float)oled_target.texture.width, (float)-oled_target.texture.height };
        Rectangle destRec = { (screenWidth - 640) / 2.0f, 85.0f, 640.0f, 320.0f };
        DrawTexturePro(oled_target.texture, sourceRec, destRec, { 0, 0 }, 0.0f, WHITE);
        DrawRectangleLines((int)destRec.x - 2, (int)destRec.y - 2, (int)destRec.width + 4, (int)destRec.height + 4, DARKGRAY);

        DrawRectangle(40, 420, 720, 95, Color{ 30, 30, 38, 255 });
        DrawRectangleLines(40, 420, 720, 95, DARKGRAY);

        DrawText("CONTROLS:", 55, 432, 12, ORANGE);
        DrawText(TextFormat("[E] Click Encoder (Active Page: P%d)", state.active_page + 1), 55, 452, 12, GREEN);
        DrawText("[W/S or Up/Down] Adjust Active Parameter", 55, 472, 12, LIGHTGRAY);
        DrawText("[T / Space] Tap Tempo", 370, 452, 12, LIGHTGRAY);
        DrawText("[1-3] Expression Pedal ADC", 370, 472, 12, LIGHTGRAY);
        DrawText("[ESC] Quit", 680, 452, 12, RED);

        EndDrawing();
    }

    UnloadRenderTexture(oled_target);
    CloseWindow();
    return 0;
}
