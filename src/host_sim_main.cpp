#include <ncurses.h>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

#include "hal/hal_display.hpp"
#include "hal/hal_encoder.hpp"
#include "hal/hal_adc.hpp"
#include "core/modulation_engine.hpp"
#include "storage/preset_manager.hpp"

using namespace phasebox::hal;
using namespace phasebox::core;
using namespace phasebox::storage;

int main() {
    // Initialize ncurses screen
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE); // non-blocking input
    curs_set(0);           // hide cursor

    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);   // Header / OLED border
        init_pair(2, COLOR_GREEN, COLOR_BLACK);  // Live stats
        init_pair(3, COLOR_YELLOW, COLOR_BLACK); // MIDI output log
        init_pair(4, COLOR_WHITE, COLOR_BLACK);  // Controls legend
    }

    // Core Phase Box C++ instances
    HalDisplay display;
    HalEncoder encoder;
    HalAdc adc;
    ModulationEngine engine;
    PresetManager preset_mgr;

    display.init(6, 7);
    encoder.init(2, 3, 4);
    adc.init(26);
    engine.set_bpm(120.0f);

    uint32_t simulated_adc_raw = 2048; // 50% pedal
    uint32_t last_turn_time_ms = 0;
    uint32_t last_tap_time_ms = 0;
    
    std::vector<std::string> midi_log;
    bool running = true;
    auto start_time = std::chrono::steady_clock::now();

    while (running) {
        auto now = std::chrono::steady_clock::now();
        uint32_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        uint32_t elapsed_us = elapsed_ms * 1000;

        // --- 1. Handle Key Input & Hardware Simulation ---
        int ch = getch();
        if (ch != ERR) {
            if (ch == 'q' || ch == 'Q' || ch == 27) {
                running = false;
            } else if (ch == KEY_LEFT || ch == 'a' || ch == 'A') {
                // Rotary Encoder Left (Counter-Clockwise)
                uint32_t delta_t = elapsed_ms - last_turn_time_ms;
                last_turn_time_ms = elapsed_ms;

                // Dynamic Acceleration: Fast turns (<35ms) step by 5.0, slow turns step by 0.1 / 1.0
                float step = (delta_t < 35) ? 5.0f : (engine.get_bpm() <= 10.0f ? 0.1f : 1.0f);
                float new_bpm = std::max(0.1f, engine.get_bpm() - step);
                engine.set_bpm(new_bpm);
                encoder.update(elapsed_us);
            } else if (ch == KEY_RIGHT || ch == 'd' || ch == 'D') {
                // Rotary Encoder Right (Clockwise)
                uint32_t delta_t = elapsed_ms - last_turn_time_ms;
                last_turn_time_ms = elapsed_ms;

                float step = (delta_t < 35) ? 5.0f : (engine.get_bpm() < 10.0f ? 0.1f : 1.0f);
                float new_bpm = std::min(300.0f, engine.get_bpm() + step);
                engine.set_bpm(new_bpm);
                encoder.update(elapsed_us);
            } else if (ch == ' ' || ch == '
' || ch == 'e' || ch == 'E') {
                // Encoder Button Click
                display.draw_pixel(0, 0, true); // Mock visual feedback
            } else if (ch == 't' || ch == 'T') {
                // Tap Tempo Switch
                if (last_tap_time_ms > 0) {
                    uint32_t interval = elapsed_ms - last_tap_time_ms;
                    if (interval >= 200 && interval <= 3000) { // 20 BPM to 300 BPM
                        float tap_bpm = 60000.0f / static_cast<float>(interval);
                        engine.set_bpm(tap_bpm);
                    }
                }
                last_tap_time_ms = elapsed_ms;
            } else if (ch == KEY_UP || ch == 'w' || ch == 'W') {
                // Expression Pedal Up
                simulated_adc_raw = std::min(4095u, simulated_adc_raw + 128);
                adc.set_simulated_raw(simulated_adc_raw);
            } else if (ch == KEY_DOWN || ch == 's' || ch == 'S') {
                // Expression Pedal Down
                simulated_adc_raw = (simulated_adc_raw >= 128) ? simulated_adc_raw - 128 : 0;
                adc.set_simulated_raw(simulated_adc_raw);
            }
        }

        // --- 2. Tick Engine & Render Framebuffer ---
        auto msgs = engine.tick(50000); // 50ms tick
        for (const auto& msg : msgs) {
            char log_buf[64];
            snprintf(log_buf, sizeof(log_buf), "MIDI CC [0x%02X] Channel: %d | CC# %d | Value: %3d",
                     msg.status, (msg.status & 0x0F) + 1, msg.data1, msg.data2);
            midi_log.push_back(log_buf);
            if (midi_log.size() > 6) midi_log.erase(midi_log.begin());
        }

        display.render();

        // --- 3. Render Terminal UI via ncurses ---
        erase();
        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(0, 2, "=== PHASE BOX: NATIVE MAC C++ TERMINAL SIMULATOR ===");
        attroff(COLOR_PAIR(1) | A_BOLD);

        // System Parameters Status Line
        attron(COLOR_PAIR(2));
        mvprintw(2, 2, "BPM: %6.1f  |  Exp Pedal: %3d%% (RAW: %4d)  |  Render Count: %ld",
                 engine.get_bpm(), (simulated_adc_raw * 100) / 4095, simulated_adc_raw, display.get_render_count());
        attroff(COLOR_PAIR(2));

        // Simulated OLED Framebuffer Box (128x32 pixel matrix sampled to ASCII)
        attron(COLOR_PAIR(1));
        mvprintw(4, 2, "+---------------------------------------------------------------------------------------------------------------------------------+");
        mvprintw(5, 2, "| [OLED 128x32 SSD1306 Display Framebuffer]                                                                                        |");
        
        // Draw ASCII representation of framebuffer
        const auto& buffer = display.get_buffer();
        for (int r = 0; r < 8; ++r) {
            move(6 + r, 2);
            addch('|');
            addch(' ');
            for (int c = 0; c < 128; ++c) {
                uint8_t byte = buffer[r * 128 + c];
                addch(byte ? '#' : '.');
            }
            addch(' ');
            addch('|');
        }
        mvprintw(14, 2, "+---------------------------------------------------------------------------------------------------------------------------------+");
        attroff(COLOR_PAIR(1));

        // MIDI CC Output Transmission Stream
        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(16, 2, "Live MIDI CC Output Stream (31,250 Baud UART):");
        attroff(COLOR_PAIR(3) | A_BOLD);
        attron(COLOR_PAIR(3));
        for (size_t i = 0; i < midi_log.size(); ++i) {
            mvprintw(17 + i, 4, "> %s", midi_log[i].c_str());
        }
        attroff(COLOR_PAIR(3));

        // Controls Legend Footer
        attron(COLOR_PAIR(4));
        mvprintw(24, 2, "CONTROLS: [<-/->] or [A/D] Turn Encoder (Dynamic Speed) | [SPACE] Click | [T] Tap Tempo");
        mvprintw(25, 2, "          [^/v] or [W/S] Expression Pedal | [Q] Quit Simulator");
        attroff(COLOR_PAIR(4));

        refresh();
        std::this_thread::sleep_for(std::chrono::milliseconds(40)); // ~25 FPS
    }

    endwin(); // Restore terminal settings
    return 0;
}
