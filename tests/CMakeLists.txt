cmake_minimum_required(VERSION 3.14)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(run_host_tests
    test_main.cpp
    test_macro_engine.cpp
    test_hal_adc.cpp
    test_hal_encoder.cpp
    test_hal_display.cpp
    test_preset_manager.cpp
    ../src/hal/hal_adc.cpp
    ../src/hal/hal_encoder.cpp
    ../src/hal/hal_display.cpp
    ../src/storage/preset_manager.cpp
)

# Enable host-build simulation code in headers/sources
target_compile_definitions(run_host_tests PRIVATE HOST_BUILD)

target_include_directories(run_host_tests PRIVATE ../include)
target_link_libraries(run_host_tests gtest_main)

include(GoogleTest)
gtest_discover_tests(run_host_tests)