add_executable(mvt_layout_dump
    LayoutDump/main.cpp
)

add_executable(mvt_render_preview
    RenderPreview/main.cpp
)

target_link_libraries(mvt_layout_dump
    PRIVATE
        mvt_layout
        mvt_theme
        juce::juce_core
        juce::juce_recommended_config_flags
)

target_include_directories(mvt_layout_dump PRIVATE "${PROJECT_SOURCE_DIR}")
mvt_set_common_properties(mvt_layout_dump)

target_link_libraries(mvt_render_preview
    PRIVATE
        mvt_ui
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_recommended_config_flags
)

target_include_directories(mvt_render_preview PRIVATE "${PROJECT_SOURCE_DIR}")
mvt_set_common_properties(mvt_render_preview)
