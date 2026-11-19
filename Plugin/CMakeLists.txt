juce_add_plugin(MvtPlugin
    COMPANY_NAME "Stimuli"
    BUNDLE_ID "com.stimuli.plugin"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Stml
    PLUGIN_CODE Stm1
    PRODUCT_NAME "Stimuli"
    VST3_CATEGORIES "Fx Analyzer"
    FORMATS VST3 Standalone
)

target_sources(MvtPlugin PRIVATE
    ControlStrip.cpp
    ControlStrip.h
    PluginParameters.h
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
)

target_compile_definitions(MvtPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(MvtPlugin
    PRIVATE
        mvt_core
        mvt_layout
        mvt_theme
        mvt_dsp
        mvt_ui
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_recommended_config_flags
)

target_include_directories(MvtPlugin PRIVATE "${PROJECT_SOURCE_DIR}")
mvt_set_project_warnings(MvtPlugin)

# Keep plugin binaries inside the build tree by default. This avoids writing to
# system-wide VST3 folders during development.
set_target_properties(MvtPlugin PROPERTIES
    JUCE_VST3_COPY_DIR "${CMAKE_BINARY_DIR}/VST3"
    JUCE_STANDALONE_COPY_DIR "${CMAKE_BINARY_DIR}/Standalone"
)

# Keep a usable default skin next to the standalone binary during development.
add_custom_command(TARGET MvtPlugin_Standalone POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/Assets"
            "$<TARGET_FILE_DIR:MvtPlugin_Standalone>/Assets"
    VERBATIM
)
