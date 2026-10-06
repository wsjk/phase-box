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
    UIController ui;

    adc.init();
    encoder.init();
    display.init(6, 7);
    preset_mgr.init();
    ui.init(&engine, &display, &preset_mgr);

    uint32_t now_us = 0;
    bool running = true;
    int raw_adc = 2048;

    while (running) {
        int ch = getch();
        int encoder_delta = 0;
        bool encoder_click = false;

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
            case ' ':
            case '\n':
            case KEY_ENTER:
                encoder_click = true;
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
            ui.handle_encoder_input(encoder_delta, now_us);
        }

        if (encoder_click) {
            ui.handle_button_press(false, now_us / 1000);
        }

        now_us += 10000;
        auto msgs = engine.tick(10000);

        erase();
        mvprintw(0, 0, "=== PHASE BOX C++ TERMINAL SIMULATOR ===");
        
        mvprintw(2, 0, "BPM: %.1f | Expression Pedal ADC: %d / 4095 (%.0f%%)", 
                 engine.get_bpm(), raw_adc, (raw_adc / 4095.0f) * 100.0f);

        mvprintw(4, 0, "Display Framebuffer Status: [ Render Count: %u ]", 
                 display.get_render_count());

        mvprintw(6, 0, "Controls:");
        mvprintw(7, 2, "Left/Right (A/D) : Rotary Encoder Turn");
        mvprintw(8, 2, "Space/Enter      : Encoder Click");
        mvprintw(9, 2, "Up/Down (W/S)    : Expression Pedal Sweep");
        mvprintw(10, 2, "T                : Tap Tempo");
        mvprintw(11, 2, "Q / ESC          : Quit");

        if (!msgs.empty()) {
            mvprintw(13, 0, "Latest Outgoing MIDI CC: Status=0x%02X Data1=%d Data2=%d",
                     msgs.status, msgs.data1, msgs.data2);
        }

        refresh();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    endwin();
    return 0;
}
