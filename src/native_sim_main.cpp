#include "raylib.h"
#include <cmath>
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

class PhaseOscillator {
public:
    enum WaveShape { SINE = 0, SAW = 1, SQUARE = 2, TRIANGLE = 3 };

    WaveShape shape = SINE;
    float bpm = 120.0f;
    float phase_offset = 0.0f;
    float phase = 0.0f;

    void update(float dt) {
        float freq = (bpm / 60.0f) * 2.0f;
        phase += freq * dt * static_cast<float>(M_PI);
        if (phase >= 2.0f * static_cast<float>(M_PI)) {
            phase -= 2.0f * static_cast<float>(M_PI);
        }
    }

    float evaluateCC(float t_offset, float adc_expression) {
        float t = phase + t_offset + phase_offset;
        t = std::fmod(t, 2.0f * static_cast<float>(M_PI));
        if (t < 0.0f) t += 2.0f * static_cast<float>(M_PI);

        float val = 0.0f;
        switch (shape) {
            case SINE: val = std::sin(t); break;
            case SAW: val = 1.0f - (t / static_cast<float>(M_PI)); break;
            case SQUARE: val = (t < static_cast<float>(M_PI)) ? 1.0f : -1.0f; break;
            case TRIANGLE: {
                float norm = t / (2.0f * static_cast<float>(M_PI));
                val = 2.0f * std::abs(2.0f * (norm - std::floor(norm + 0.5f))) - 1.0f;
                break;
            }
        }
        val *= (0.5f + 0.5f * adc_expression);
        return (val * 0.5f) + 0.5f;
    }
};

struct PhaseBoxState {
    PhaseOscillator osc;
    int active_page = 0;        // 0: Wave, 1: BPM, 2: Phase, 3: MIDI Chan, 4: MIDI CC
    bool button_state = false;
    float adc_expression = 0.0f;
    int midi_channel = 1;       // 1 to 16
    int midi_cc_num = 16;       // 0 to 127
    int last_sent_cc_val = -1;
};

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

void DrawOLEDDisplay(RenderTexture2D& oled_target, PhaseBoxState& state) {
    BeginTextureMode(oled_target);
    ClearBackground(BLACK);

    DrawRectangleLines(0, 0, 128, 64, WHITE);
    DrawLine(0, 14, 128, 14, WHITE);
    DrawLine(0, 50, 128, 50, WHITE);

    const char* page_names[] = {"P1: WAVE", "P2: BPM", "P3: PHASE", "P4: CHAN", "P5: CC"};
    std::string header = std::string(page_names[state.active_page]) + (state.button_state ? "*" : "");
    DrawText(header.c_str(), 4, 3, 10, WHITE);

    const int wave_width = 120;
    const int wave_height = 33;
    const int start_x = 4;
    const int start_y = 16;

    std::vector<Vector2> points;
    for (int x = 0; x < wave_width; ++x) {
        float sample_offset = (static_cast<float>(x) / wave_width) * 2.0f * static_cast<float>(M_PI);
        float cc_val = state.osc.evaluateCC(sample_offset, state.adc_expression);
        float py = start_y + wave_height - (cc_val * wave_height);
        points.push_back({(float)(start_x + x), py});
    }

    for (size_t i = 1; i < points.size(); ++i) {
        DrawLineV(points[i-1], points[i], WHITE);
    }

    std::string footer = "";
    const char* wave_names[] = {"SINE", "SAW", "SQR", "TRI"};
    if (state.active_page == 0) {
        footer = "VAL: " + std::string(wave_names[state.osc.shape]);
    } else if (state.active_page == 1) {
        footer = "VAL: " + std::to_string(static_cast<int>(state.osc.bpm)) + " BPM";
    } else if (state.active_page == 2) {
        footer = "VAL: " + std::string(TextFormat("%.2f", state.osc.phase_offset));
    } else if (state.active_page == 3) {
        footer = "VAL: CH " + std::to_string(state.midi_channel);
    } else if (state.active_page == 4) {
        footer = "VAL: CC " + std::to_string(state.midi_cc_num);
    }
    DrawText(footer.c_str(), 4, 53, 8, WHITE);

    EndTextureMode();
}

int main() {
    const int screenWidth = 800;
    const int screenHeight = 560;
    InitWindow(screenWidth, screenHeight, "Phase Box - Configurable MIDI Generator");
    SetTargetFPS(60);

    PhaseBoxState state;
    std::vector<std::chrono::steady_clock::time_point> tap_times;

#ifdef __APPLE__
    MIDIClientCreate(CFSTR("PhaseBox MIDI Generator"), NULL, NULL, &g_midiClient);
    MIDISourceCreate(g_midiClient, CFSTR("Phase Box Virtual Out"), &g_virtualOutput);
#endif

    RenderTexture2D oled_target = LoadRenderTexture(128, 64);
    auto last_cc_time = std::chrono::steady_clock::now();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_E)) {
            state.button_state = !state.button_state;
            if (state.button_state) {
                state.active_page = (state.active_page + 1) % 5;
            }
        }

        bool inc = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP);
        bool dec = IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN);

        if (inc) {
            if (state.active_page == 0) state.osc.shape = static_cast<PhaseOscillator::WaveShape>((state.osc.shape + 1) % 4);
            else if (state.active_page == 1) state.osc.bpm = std::min(300.0f, state.osc.bpm + 5.0f);
            else if (state.active_page == 2) state.osc.phase_offset += 0.2f;
            else if (state.active_page == 3) state.midi_channel = std::min(16, state.midi_channel + 1);
            else if (state.active_page == 4) state.midi_cc_num = std::min(127, state.midi_cc_num + 1);
        }
        if (dec) {
            if (state.active_page == 0) state.osc.shape = static_cast<PhaseOscillator::WaveShape>((state.osc.shape + 3) % 4);
            else if (state.active_page == 1) state.osc.bpm = std::max(40.0f, state.osc.bpm - 5.0f);
            else if (state.active_page == 2) state.osc.phase_offset -= 0.2f;
            else if (state.active_page == 3) state.midi_channel = std::max(1, state.midi_channel - 1);
            else if (state.active_page == 4) state.midi_cc_num = std::max(0, state.midi_cc_num - 1);
        }

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

        state.osc.update(dt);

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_cc_time).count() > 30) {
            float current_sample = state.osc.evaluateCC(0.0f, state.adc_expression);
            int midi_val = static_cast<int>(current_sample * 127.0f);
            midi_val = std::clamp(midi_val, 0, 127);

            #ifdef __APPLE__
            SendMIDIControlChange(state.midi_channel, state.midi_cc_num, midi_val);
            #endif

            state.last_sent_cc_val = midi_val;
            last_cc_time = now;
        }

        DrawOLEDDisplay(oled_target, state);

        BeginDrawing();
        ClearBackground(Color{ 20, 20, 25, 255 });

        DrawText("PHASE BOX - MIDI LFO GENERATOR SIMULATOR", 40, 20, 20, RAYWHITE);
        DrawText(TextFormat("Streaming CH:%d | CC:%d | Value:%d", state.midi_channel, state.midi_cc_num, state.last_sent_cc_val), 40, 45, 12, GREEN);

        Rectangle sourceRec = { 0.0f, 0.0f, (float)oled_target.texture.width, (float)-oled_target.texture.height };
        Rectangle destRec = { (screenWidth - 640) / 2.0f, 75.0f, 640.0f, 320.0f };
        DrawTexturePro(oled_target.texture, sourceRec, destRec, { 0, 0 }, 0.0f, WHITE);
        DrawRectangleLines((int)destRec.x - 2, (int)destRec.y - 2, (int)destRec.width + 4, (int)destRec.height + 4, DARKGRAY);

        DrawRectangle(40, 410, 720, 125, Color{ 30, 30, 38, 255 });
        DrawRectangleLines(40, 410, 720, 125, DARKGRAY);

        DrawText("CONTROLS & CONFIGURATION:", 55, 422, 12, ORANGE);
        DrawText(TextFormat("[E] Click Encoder (Active Page: P%d - %s)", state.active_page + 1, 
                  state.active_page == 0 ? "WAVE" : state.active_page == 1 ? "BPM" : state.active_page == 2 ? "PHASE" : state.active_page == 3 ? "CHAN" : "CC"), 55, 442, 12, LIGHTGRAY);
        DrawText("[W/S or Up/Down] Adjust Active Parameter Value", 55, 462, 12, LIGHTGRAY);
        DrawText("[T / Space] Tap Tempo  |  [1-3] Expression", 55, 482, 12, LIGHTGRAY);
        DrawText("Outputs to 'Phase Box Virtual Out'", 55, 505, 12, YELLOW);
        DrawText("[ESC] Quit", 680, 442, 12, RED);

        EndDrawing();
    }

    UnloadRenderTexture(oled_target);
    CloseWindow();
    return 0;
}
