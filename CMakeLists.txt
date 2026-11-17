cmake_minimum_required(VERSION 3.22)

project(MvtSuite
    VERSION 0.1.0
    DESCRIPTION "Audio-reactive visualization suite and engine-first UI designer"
    LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(MvtOptions)
include(MvtHelpers)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD ${MVT_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# ---------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS   OFF CACHE BOOL "" FORCE)

set(MVT_JUCE_SOURCE_DIR "" CACHE PATH "Use a local JUCE checkout instead of downloading it")

if (MVT_JUCE_SOURCE_DIR)
    message(STATUS "Using local JUCE at ${MVT_JUCE_SOURCE_DIR}")
    add_subdirectory("${MVT_JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.5
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Catch2 (headless tests only)
# ---------------------------------------------------------------------------
if (MVT_BUILD_TESTS)
    set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
    set(MVT_CATCH2_SOURCE_DIR "" CACHE PATH "Use a local Catch2 checkout instead of downloading it")

    if (MVT_CATCH2_SOURCE_DIR)
        add_subdirectory("${MVT_CATCH2_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/Catch2" EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare(Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG        v3.16.1
            GIT_SHALLOW    TRUE
            GIT_PROGRESS   TRUE
        )
        FetchContent_MakeAvailable(Catch2)
    endif()
endif()

add_subdirectory(Engine)
add_subdirectory(UIEngine)
add_subdirectory(Tools)

if (MVT_BUILD_PLUGIN)
    add_subdirectory(Plugin)
endif()

if (MVT_BUILD_DESIGNER)
    add_subdirectory(Designer)
endif()

if (MVT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

message(STATUS "MVT: C++${MVT_CXX_STANDARD}, plugin=${MVT_BUILD_PLUGIN}, designer=${MVT_BUILD_DESIGNER}, tests=${MVT_BUILD_TESTS}, export=${MVT_ENABLE_EXPORT}")
