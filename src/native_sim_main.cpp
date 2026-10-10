#include "raylib.h"
#include <cmath>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Core DSP Oscillator Engine
class PhaseOscillator {
public:
    enum WaveShape { SINE = 0, SAW = 1, SQUARE = 2, TRIANGLE = 3 };

    WaveShape shape = SINE;
    float bpm = 120.0f;
    float phase_offset = 0.0f;
    float phase = 0.0f;

    // Process a single sample step given delta time
    void update(float dt) {
        // Frequency derived from BPM (e.g., 1 beat = 1 cycle or tempo-synced LFO rate)
        float freq = (bpm / 60.0f) * 2.0f; // 2 Hz base at 120 BPM
        phase += freq * dt * static_cast<float>(M_PI);
        if (phase >= 2.0f * static_cast<float>(M_PI)) {
            phase -= 2.0f * static_cast<float>(M_PI);
        }
    }

    // Evaluate waveform sample at a specific phase offset
    float evaluate(float t_offset, float adc_expression) {
        float t = phase + t_offset + phase_offset;
        // Normalize t within [0, 2*PI]
        t = std::fmod(t, 2.0f * static_cast<float>(M_PI));
        if (t < 0.0f) t += 2.0f * static_cast<float>(M_PI);

        float val = 0.0f;
        switch (shape) {
            case SINE:
                val = std::sin(t);
                break;
            case SAW:
                val = 1.0f - (t / static_cast<float>(M_PI));
                break;
            case SQUARE:
                val = (t < static_cast<float>(M_PI)) ? 1.0f : -1.0f;
                break;
            case TRIANGLE: {
                float norm = t / (2.0f * static_cast<float>(M_PI));
                val = 2.0f * std::abs(2.0f * (norm - std::floor(norm + 0.5f))) - 1.0f;
                break;
            }
        }

        // Apply expression pedal modulation
        val *= (0.5f + 0.5f * adc_expression);
        return val;
    }
};

// Simulated Hardware State with Menu Pages
struct PhaseBoxState {
    PhaseOscillator osc;
    int active_page = 0;        // 0: Waveform, 1: BPM, 2: Phase Offset
    bool button_state = false;  // Encoder Push Button State
    float adc_expression = 0.0f;// Expression Pedal ADC input (0.0 to 1.0)
    int encoder_val = 50;       // Simulated rotary encoder detents
};

// Draw 128x64 OLED Virtual Buffer driven by DSP Engine
void DrawOLEDDisplay(RenderTexture2D& oled_target, PhaseBoxState& state) {
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

    // Waveform Viewport (Rows 15 to 49) - Render DSP Buffer
    const int wave_width = 120;
    const int wave_height = 33;
    const int start_x = 4;
    const int start_y = 16;

    std::vector<Vector2> points;
    for (int x = 0; x < wave_width; ++x) {
        float sample_phase_offset = (static_cast<float>(x) / wave_width) * 2.0f * static_cast<float>(M_PI);
        float val = state.osc.evaluate(sample_phase_offset, state.adc_expression);

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
        footer = "VAL: " + std::string(wave_names[state.osc.shape]);
    } else if (state.active_page == 1) {
        footer = "VAL: " + std::to_string(static_cast<int>(state.osc.bpm)) + " BPM";
    } else {
        footer = "VAL: " + std::string(TextFormat("%.2f", state.osc.phase_offset));
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
    std::vector<std::chrono::steady_clock::time_point> tap_times;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

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
            if (state.active_page == 0) state.osc.shape = static_cast<PhaseOscillator::WaveShape>((state.osc.shape + 1) % 4);
            else if (state.active_page == 1) state.osc.bpm = std::min(300.0f, state.osc.bpm + 5.0f);
            else if (state.active_page == 2) state.osc.phase_offset += 0.2f;
            state.encoder_val = std::min(100, state.encoder_val + 5);
        }
        if (dec) {
            if (state.active_page == 0) state.osc.shape = static_cast<PhaseOscillator::WaveShape>((state.osc.shape + 3) % 4);
            else if (state.active_page == 1) state.osc.bpm = std::max(40.0f, state.osc.bpm - 5.0f);
            else if (state.active_page == 2) state.osc.phase_offset -= 0.2f;
            state.encoder_val = std::max(0, state.encoder_val - 5);
        }

        if (IsKeyPressed(KEY_ONE)) state.adc_expression = 0.0f;
        if (IsKeyPressed(KEY_TWO)) state.adc_expression = 0.5f;
        if (IsKeyPressed(KEY_THREE)) state.adc_expression = 1.0f;

        // Tap Tempo via 'T' or Spacebar
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

        // --- UPDATE DSP ENGINE ---
        state.osc.update(dt);

        // Render OLED pixel buffer
        DrawOLEDDisplay(oled_target, state);

        // --- DRAWING TO NATIVE WINDOW ---
        BeginDrawing();
        ClearBackground(Color{ 20, 20, 25, 255 });

        DrawText("PHASE BOX EMBEDDED SIMULATOR", 40, 25, 20, RAYWHITE);
        DrawText("Hardware Benchtop & DSP Oscillator Engine", 40, 50, 14, GRAY);

        // Draw Scaled OLED Screen in Center (640x320)
        Rectangle sourceRec = { 0.0f, 0.0f, (float)oled_target.texture.width, (float)-oled_target.texture.height };
        Rectangle destRec = { (screenWidth - 640) / 2.0f, 85.0f, 640.0f, 320.0f };
        DrawTexturePro(oled_target.texture, sourceRec, destRec, { 0, 0 }, 0.0f, WHITE);
        DrawRectangleLines((int)destRec.x - 2, (int)destRec.y - 2, (int)destRec.width + 4, (int)destRec.height + 4, DARKGRAY);

        // Control Panel Help
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
