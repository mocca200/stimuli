cmake_minimum_required(VERSION 3.22)

# Catch2 is brought in by the root CMakeLists before this directory.
add_executable(mvt_tests
    TestMain.cpp
    AllocationProbe.cpp
    CoreTests.cpp
    LayoutTests.cpp
    ThemeTests.cpp
    DspTests.cpp
    SafetyTests.cpp
    LayoutWriterTests.cpp
    DesignerTests.cpp
)

target_link_libraries(mvt_tests
    PRIVATE
        mvt_core
        mvt_layout
        mvt_theme
        mvt_dsp
        mvt_ui
        Catch2::Catch2WithMain
        juce::juce_core
        juce::juce_graphics
        juce::juce_recommended_config_flags
)

target_include_directories(mvt_tests PRIVATE "${PROJECT_SOURCE_DIR}")
mvt_set_common_properties(mvt_tests)

include(Catch)
catch_discover_tests(mvt_tests)

add_test(NAME mvt_all COMMAND mvt_tests)
