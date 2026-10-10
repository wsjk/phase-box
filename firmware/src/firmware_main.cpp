#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "PhaseOscillator.hpp"

void core1_entry() {
while (true) {
    tight_loop_contents();
}
}

int main() {
stdio_init_all();
multicore_launch_core1(core1_entry);

PhaseOscillator osc;
absolute_time_t next_frame = get_absolute_time();
const uint32_t dt_us = 16667;

while (true) {
    osc.update(0.0166f);
    next_frame = delayed_by_us(next_frame, dt_us);
    sleep_until(next_frame);
}
}
