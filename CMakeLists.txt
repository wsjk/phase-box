cmake_minimum_required(VERSION 3.13)

# Option to toggle between Host Simulation (macOS) and Target Build (RP2040)
option(BUILD_HOST_TESTS "Build host unit tests on macOS using GoogleTest" ON)

if(NOT BUILD_HOST_TESTS)
    # Target build: Pico SDK Initialization
    include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
    project(phase_box C CXX ASM)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_CXX_STANDARD 17)
    pico_sdk_init()
else()
    # Host build: Native macOS binary
    project(phase_box_host CXX)
    set(CMAKE_CXX_STANDARD 17)

    # Fetch GoogleTest automatically on host build
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
endif()

# Include public headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Subdirectories for code modules
if(BUILD_HOST_TESTS)
    add_subdirectory(tests)
else()
    # Pico hardware executable targets go here
enable_testing()
endif()
