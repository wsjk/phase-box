add_test([=[PhaseLFOTest.WaveformSineLUT]=]  /Users/ww/src/phase-box/build-host/tests/run_host_tests [==[--gtest_filter=PhaseLFOTest.WaveformSineLUT]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PhaseLFOTest.WaveformSineLUT]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/Users/ww/src/phase-box/tests/test_main.cpp:9]==]
    WORKING_DIRECTORY [==[/Users/ww/src/phase-box/build-host/tests]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[PhaseLFOTest.WaveformSquare]=]  /Users/ww/src/phase-box/build-host/tests/run_host_tests [==[--gtest_filter=PhaseLFOTest.WaveformSquare]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PhaseLFOTest.WaveformSquare]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/Users/ww/src/phase-box/tests/test_main.cpp:19]==]
    WORKING_DIRECTORY [==[/Users/ww/src/phase-box/build-host/tests]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[PhaseLFOTest.PhaseReset]=]  /Users/ww/src/phase-box/build-host/tests/run_host_tests [==[--gtest_filter=PhaseLFOTest.PhaseReset]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PhaseLFOTest.PhaseReset]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/Users/ww/src/phase-box/tests/test_main.cpp:28]==]
    WORKING_DIRECTORY [==[/Users/ww/src/phase-box/build-host/tests]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[ClockManagerTest.DefaultInternalTap]=]  /Users/ww/src/phase-box/build-host/tests/run_host_tests [==[--gtest_filter=ClockManagerTest.DefaultInternalTap]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ClockManagerTest.DefaultInternalTap]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/Users/ww/src/phase-box/tests/test_main.cpp:39]==]
    WORKING_DIRECTORY [==[/Users/ww/src/phase-box/build-host/tests]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[ClockManagerTest.TapTempoCalculation]=]  /Users/ww/src/phase-box/build-host/tests/run_host_tests [==[--gtest_filter=ClockManagerTest.TapTempoCalculation]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ClockManagerTest.TapTempoCalculation]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/Users/ww/src/phase-box/tests/test_main.cpp:45]==]
    WORKING_DIRECTORY [==[/Users/ww/src/phase-box/build-host/tests]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[ClockManagerTest.ExternalMIDIClockFallback]=]  /Users/ww/src/phase-box/build-host/tests/run_host_tests [==[--gtest_filter=ClockManagerTest.ExternalMIDIClockFallback]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ClockManagerTest.ExternalMIDIClockFallback]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/Users/ww/src/phase-box/tests/test_main.cpp:52]==]
    WORKING_DIRECTORY [==[/Users/ww/src/phase-box/build-host/tests]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
set(run_host_tests_TESTS [==[PhaseLFOTest.WaveformSineLUT]==] [==[PhaseLFOTest.WaveformSquare]==] [==[PhaseLFOTest.PhaseReset]==] [==[ClockManagerTest.DefaultInternalTap]==] [==[ClockManagerTest.TapTempoCalculation]==] [==[ClockManagerTest.ExternalMIDIClockFallback]==])
