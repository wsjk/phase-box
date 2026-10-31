### Master Integration Plan: Enhanced Phase Box Firmware

#### Phase 1: USB MIDI Class Compliance (TinyUSB)

* **Objective:** Enable class-compliant USB MIDI communication so the Pico communicates directly with DAWs, iPads, and USB-host hardware synths.
* **Tasks:**
* Configure CMake to initialize the TinyUSB stack (`pico_stdio_usb` / TinyUSB device headers).
* Implement USB MIDI descriptors (`tusb_config.h`, `usb_descriptors.c/h`) supporting standard USB MIDI streaming endpoints.
* Update the Core 0 output routing to transmit standard 3-byte MIDI Control Change messages over `tusb_midi_packet_write`.



#### Phase 2: Polyrhythmic Clock Divisions Per Macro

* **Objective:** Transform the `MacroEngine` from a synchronized multitrack output into a polyrhythmic generative engine.
* **Tasks:**
* Extend `MacroDestination` in `MacroEngine.hpp` with a `clock_divider` property (e.g., multiplier/subdivision ratios like $1/1$, $3/4$, $2/1$, $1/2$).
* Track independent phase accumulators or step counters per macro destination inside the evaluation loop.
* Allow live configuration of macro clock multipliers via an expanded OLED menu page (`P10: MAC DIV`).



#### Phase 3: LittleFS Flash Persistence & Auto-Save Recovery

* **Objective:** Ensure performance states and presets survive power cycles, backed by an auto-save dirty-state timer.
* **Tasks:**
* Configure LittleFS on the RP2040 flash memory using the Pico SDK flash storage API.
* Create a `PresetManager` class to serialize `PhaseBoxState`, `TuringMachine`, and `MacroEngine` parameters into compact JSON or binary preset files (`preset_01.bin`).
* Implement a background "dirty state" watcher that automatically triggers a non-blocking flash write 2 seconds after the user finishes tweaking parameters.



#### Phase 4: OLED Screen Burn-In Protection & Screensaver

* **Objective:** Protect the SSD1306 OLED display during long live performances and stationary studio sessions.
* **Tasks:**
* Track user input inactivity time on Core 1 using `absolute_time_t`.
* Trigger a low-power screensaver mode (dimming the display contrast register or initiating a gentle pixel-orbit pattern) after 30 seconds of encoder/button inactivity.
* Instantly restore full brightness upon any encoder turn or button press.



---

### Suggested Execution Order

1. **Per-Macro Clock Divisions** (builds directly on our existing C++ DSP math).
2. **TinyUSB MIDI Integration** (establishes actual hardware output).
3. **LittleFS & Auto-Save** (adds robust storage persistence).
4. **OLED Screensaver & Burn-In Protection** (polishes the hardware UI/UX).