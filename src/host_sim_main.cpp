#include <ncurses.h>
#include <chrono>
#include <thread>
#include <vector>

#include "hal/hal_adc.hpp"
#include "hal/hal_encoder.hpp"
#include "hal/hal_display.hpp"
#include "core/modulation_engine.hpp"
#include "storage/preset_manager.hpp"
#include "ui/ui_controller.hpp"

using namespace phasebox::hal;
using namespace phasebox::core;
using namespace phasebox::storage;
using namespace phasebox::ui;

int main() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);

    HalAdc adc;
    HalEncoder encoder;
    HalDisplay display;
    ModulationEngine engine;
    PresetManager preset_mgr;

    UIController ui(engine);

    adc.init();
    encoder.init();
    display.init(6, 7);
    preset_mgr.init();

    ui.init();

    engine.set_bpm(120.0f);

    uint32_t now_us = 0;
    bool running = true;
    int raw_adc = 2048;

    while (running) {
        int ch = getch();
        int encoder_delta = 0;

        switch (ch) {
            case KEY_LEFT:
            case 'a':
            case 'A':
                encoder_delta = -1;
                break;
            case KEY_RIGHT:
            case 'd':
            case 'D':
                encoder_delta = 1;
                break;
            case KEY_UP:
            case 'w':
            case 'W':
                raw_adc = (raw_adc + 256 > 4095) ? 4095 : raw_adc + 256;
                adc.set_simulated_raw(raw_adc);
                break;
            case KEY_DOWN:
            case 's':
            case 'S':
                raw_adc = (raw_adc - 256 < 0) ? 0 : raw_adc - 256;
                adc.set_simulated_raw(raw_adc);
                break;
            case 't':
            case 'T':
                ui.handle_tap_tempo(now_us / 1000);
                break;
            case 'q':
            case 'Q':
            case 27: // ESC
                running = false;
                break;
        }

        if (encoder_delta != 0) {
            ui.handle_bpm_encoder_input(encoder_delta, now_us);
        }

        now_us += 10000;
        auto msgs = engine.tick(10000);

        erase();
        mvprintw(0, 0, "=== PHASE BOX C++ TERMINAL SIMULATOR ===");
        
        mvprintw(2, 0, "Expression Pedal ADC: %d / 4095 (%.0f%%)", 
                 raw_adc, (raw_adc / 4095.0f) * 100.0f);

        mvprintw(4, 0, "Display Framebuffer Status: [ Render Count: %u ]", 
                 display.get_render_count());

        mvprintw(6, 0, "Controls:");
        mvprintw(7, 2, "Left/Right (A/D) : Rotary Encoder Turn (Adjust BPM)");
        mvprintw(8, 2, "Up/Down (W/S)    : Expression Pedal Sweep");
        mvprintw(9, 2, "T                : Tap Tempo");
        mvprintw(10, 2, "Q / ESC          : Quit");

        // Access element 0 of the returned vector using .at(0)
        if (!msgs.empty()) {
            mvprintw(12, 0, "Latest Outgoing MIDI CC: Status=0x%02X Data1=%d Data2=%d",
                     msgs.at(0).status, msgs.at(0).data1, msgs.at(0).data2);
        }

        refresh();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    endwin();
    return 0;
}
