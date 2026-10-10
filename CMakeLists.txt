cmake_minimum_required(VERSION 3.14)
project(PhaseBoxSim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch Raylib automatically via CMake FetchContent
include(FetchContent)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.0.tar.gz
)
FetchContent_MakeAvailable(raylib)

# Host terminal simulator (legacy fallback)
add_executable(phase_box_sim src/host_sim_main.cpp)

# Native windowed Raylib simulator
add_executable(phase_box_native_sim src/native_sim_main.cpp)
target_link_libraries(phase_box_native_sim PRIVATE raylib)

# Enable macOS / Linux framework and graphics linkages required by raylib
if(APPLE)
    target_link_libraries(phase_box_native_sim PRIVATE "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()
