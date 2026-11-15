cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(phase_box_firmware C CXX ASM)
include(../cmake/version.cmake)   # PHASEBOX_VERSION, from the latest v* git tag
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

# OLED rows: 32 or 64 (cmake -DPHASEBOX_OLED_HEIGHT=64 ..)
set(PHASEBOX_OLED_HEIGHT 32 CACHE STRING "OLED panel height in pixels (32 or 64)")

add_executable(phase_box_firmware
    src/firmware_main.cpp
    src/usb_descriptors.c
    ../src/PhaseOscillator.cpp
)

target_compile_definitions(phase_box_firmware PRIVATE PHASEBOX_OLED_HEIGHT=${PHASEBOX_OLED_HEIGHT})

target_include_directories(phase_box_firmware PRIVATE
    ../include
    include
)

target_link_libraries(phase_box_firmware
    pico_stdlib
    hardware_i2c
    hardware_uart
    hardware_gpio
    hardware_flash
    pico_flash
    pico_multicore
    tinyusb_device
    tinyusb_board
)

pico_set_program_version(phase_box_firmware "${PHASEBOX_VERSION}")   # shown by `picotool info`
pico_add_extra_outputs(phase_box_firmware)
