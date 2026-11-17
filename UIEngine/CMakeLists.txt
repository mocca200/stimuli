add_library(mvt_ui STATIC
    Palette.cpp
    Skin.cpp
    RenderEngine.cpp
    FrameSafetyGate.cpp
    DesignerDocument.cpp
)

target_include_directories(mvt_ui PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(mvt_ui
    PUBLIC
        mvt_core
        mvt_layout
        mvt_theme
        mvt_dsp
    PRIVATE
        juce::juce_gui_basics
        juce::juce_graphics
)
mvt_set_common_properties(mvt_ui)

# Development convenience: when no skin is found next to the executable or in
# the working directory, load the repository assets. Package builds will copy
# the skin next to the binary instead.
target_compile_definitions(mvt_ui PRIVATE
    MVT_DEV_ASSETS_DIR="${PROJECT_SOURCE_DIR}/Assets"
)
