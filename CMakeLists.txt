cmake_minimum_required(VERSION 3.13)

# Include Pico SDK setup if building for hardware
if (NOT HOST_BUILD)
    include(pico_sdk_import.cmake)
endif()

project(phase_box C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

if (NOT HOST_BUILD)
    # Target RP2040 Build Configuration
    pico_sdk_init()

    add_executable(phase_box
        src/main.cpp
        src/hal/hal_adc.cpp
        src/hal/hal_encoder.cpp
        src/hal/hal_display.cpp
        src/storage/preset_manager.cpp
        src/ui/ui_controller.cpp
        src/ui/ui_controller.cpp
    )

    target_link_libraries(phase_box
        pico_stdlib
        pico_multicore
        hardware_uart
        hardware_adc
        hardware_i2c
        hardware_gpio
        hardware_timer
    )

    pico_add_extra_outputs(phase_box)
else()
    # Host Unit Test Build Configuration
    add_subdirectory(tests)
endif()

# --- Native C++ Terminal Simulator for macOS / Host ---
if (HOST_BUILD)
    add_executable(phase_box_sim
        src/host_sim_main.cpp
        src/hal/hal_adc.cpp
        src/hal/hal_encoder.cpp
        src/hal/hal_display.cpp
        src/storage/preset_manager.cpp
    src/ui/ui_controller.cpp
    )

    target_compile_definitions(phase_box_sim PRIVATE HOST_BUILD)
    target_include_directories(phase_box_sim PRIVATE include)
    target_link_libraries(phase_box_sim ncurses)
endif()
