add_library(mvt_dsp STATIC
    Cqt.cpp
    Fft.cpp
    Hpss.cpp
    DspPipeline.cpp
)

target_include_directories(mvt_dsp PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(mvt_dsp
    PUBLIC mvt_core
)
mvt_set_common_properties(mvt_dsp)
