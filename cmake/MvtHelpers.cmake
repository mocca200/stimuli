include_guard(GLOBAL)

# Applies our warning policy to first-party targets only. Never apply this to
# JUCE targets: third-party code is not ours to police.
function(mvt_set_project_warnings target)
    if (MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /utf-8
            /Zc:__cplusplus
            /Zc:preprocessor
            /EHsc
            /fp:precise
            # Cache-line alignment on lock-free primitives intentionally pads
            # the containing structure; C4324 is expected and harmless here.
            /wd4324
        )
        if (MVT_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wconversion
            -Wsign-conversion
            -Wno-unused-parameter
        )
        if (MVT_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(mvt_enable_avx2_if_available target)
    # Deliberately opt-in: release builds may enable this, but the codebase
    # must not require AVX2 for correctness.
    if (MSVC)
        target_compile_options(${target} PRIVATE /arch:AVX2)
    else()
        target_compile_options(${target} PRIVATE -mavx2)
    endif()
endfunction()

function(mvt_set_common_properties target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD ${MVT_CXX_STANDARD}
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE YES
    )
    target_compile_definitions(${target} PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=0
    )
    mvt_set_project_warnings(${target})
endfunction()
