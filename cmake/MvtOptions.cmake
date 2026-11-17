# Global project options. Kept deliberately small and explicit.

option(MVT_BUILD_PLUGIN   "Build the VST3/Standalone plugin" ON)
option(MVT_BUILD_DESIGNER "Build the UI Designer Studio" ON)
option(MVT_BUILD_TESTS    "Build headless unit tests" ON)
option(MVT_ENABLE_EXPORT  "Build the MP4 export engine (requires FFmpeg)" OFF)
option(MVT_WARNINGS_AS_ERRORS "Treat project warnings as errors" ON)

set(MVT_CXX_STANDARD 20 CACHE STRING "C++ standard used by all project targets")

# Audio / UI defaults. These are compile definitions, not user settings.
set(MVT_COMPANY_NAME "MVT" CACHE STRING "")
set(MVT_PRODUCT_NAME "MVT Visualizer" CACHE STRING "")
