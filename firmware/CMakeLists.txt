cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(phase_box_firmware C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

# OLED rows: 32 or 64 (cmake -DPHASEBOX_OLED_HEIGHT=64 ..)
set(PHASEBOX_OLED_HEIGHT 32 CACHE STRING "OLED panel height in pixels (32 or 64)")

add_executable(phase_box_firmware
    src/firmware_main.cpp
    src/usb_descriptors.c
    ../src/PhaseOscillator.cpp
    ../src/hal/hal_adc.cpp
)

target_compile_definitions(phase_box_firmware PRIVATE PHASEBOX_OLED_HEIGHT=${PHASEBOX_OLED_HEIGHT})

target_include_directories(phase_box_firmware PRIVATE
    ../include
    include
)

target_link_libraries(phase_box_firmware
    pico_stdlib
    hardware_i2c
    hardware_adc
    hardware_uart
    hardware_gpio
    pico_multicore
    tinyusb_device
    tinyusb_board
)

pico_add_extra_outputs(phase_box_firmware)
