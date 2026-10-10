cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(phase_box_firmware C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(phase_box_firmware
src/firmware_main.cpp
../src/PhaseOscillator.cpp
)

target_include_directories(phase_box_firmware PRIVATE
../include
include
)

target_link_libraries(phase_box_firmware
pico_stdlib
hardware_i2c
pico_multicore
)

pico_add_extra_outputs(phase_box_firmware)
