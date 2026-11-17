juce_add_gui_app(MvtDesigner
    COMPANY_NAME "MVT"
    BUNDLE_ID "com.mvt.designer"
    PRODUCT_NAME "MVT Designer"
)

target_sources(MvtDesigner PRIVATE
    Main.cpp
    CanvasComponent.cpp
    HierarchyPanel.cpp
    InspectorPanel.cpp
    HotReload.cpp
)

target_compile_definitions(MvtDesigner PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(MvtDesigner
    PRIVATE
        mvt_core
        mvt_ui
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_recommended_config_flags
)

target_include_directories(MvtDesigner PRIVATE "${PROJECT_SOURCE_DIR}")
mvt_set_project_warnings(MvtDesigner)

add_custom_command(TARGET MvtDesigner POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/Assets"
            "$<TARGET_FILE_DIR:MvtDesigner>/Assets"
    VERBATIM
)
