# Core primitives are header-only. The interface target exists so every
# engine module shares the same include root and language settings.
add_library(mvt_core INTERFACE)
target_include_directories(mvt_core INTERFACE "${PROJECT_SOURCE_DIR}")
target_compile_features(mvt_core INTERFACE cxx_std_${MVT_CXX_STANDARD})
target_link_libraries(mvt_core INTERFACE
    juce::juce_core
    juce::juce_recommended_config_flags
)

add_library(mvt_layout STATIC
    Layout/LayoutParser.cpp
    Layout/LayoutWriter.cpp
    Layout/FlexSolver.cpp
)
target_include_directories(mvt_layout PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(mvt_layout
    PUBLIC mvt_core
    PRIVATE juce::juce_core
)
mvt_set_common_properties(mvt_layout)

add_library(mvt_theme STATIC
    Theme/TokenValue.cpp
    Theme/ThemeRegistry.cpp
)
target_include_directories(mvt_theme PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(mvt_theme
    PUBLIC mvt_core
    PRIVATE juce::juce_core
)
mvt_set_common_properties(mvt_theme)

# DSP is added in the next milestone; keep the option from breaking builds
# while the source tree is being brought up.
# DSP is added in the next milestone; keep the option from breaking builds
# while the source tree is being brought up.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/DSP/CMakeLists.txt")
    add_subdirectory(DSP)
endif()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Export/CMakeLists.txt")
    add_subdirectory(Export)
endif()
