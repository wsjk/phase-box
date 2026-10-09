#include <iostream>
#include <thread>
#include <chrono>

#include "core/modulation_engine.hpp"
#include "core/macro_engine.hpp"
#include "hal/hal_adc.hpp"
#include "hal/hal_display.hpp"
#include "ui/ui_controller.hpp"

using namespace phasebox::core;
using namespace phasebox::hal;
using namespace phasebox::ui;

int main() {
    std::cout << "Starting Phase Box Host Simulation...\n";

    ModulationEngine engine;
    MacroEngine macro;
    HalAdc adc;
    HalDisplay display;
    UIController ui_controller(engine);

    adc.init();
    display.init(0, 0);
    ui_controller.init();

    engine.set_bpm(120.0f);

    for (int i = 0; i < 5; ++i) {
        uint32_t fake_time_us = i * 1000; // 1ms increments
        
        float expr = adc.read_normalized();
        macro.set_expression_value(expr);
        
        auto msgs = engine.tick(fake_time_us);
        display.clear();
        display.render();

        std::cout << "Tick " << i << " | Active MIDI messages: " << msgs.size() << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    std::cout << "Host simulation finished successfully.\n";
    return 0;
}
