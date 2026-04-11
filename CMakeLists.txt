cmake_minimum_required(VERSION 3.28)
project(Earthrise LANGUAGES CXX RC)

# ── Platform guard ──────────────────────────────────────────────────────────
if(NOT WIN32)
    message(FATAL_ERROR "This project targets Windows only.")
endif()

if(NOT MSVC)
    message(FATAL_ERROR "This project requires the MSVC toolchain.")
endif()

# ── C++ standard ────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Static CRT linkage (/MT, /MTd) ─────────────────────────────────────────
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ── Common compiler flags ───────────────────────────────────────────────────
add_compile_options(
    /W4             # Warning level 4
    /sdl            # Security Development Lifecycle checks
    /permissive-    # Standards conformance
    /bigobj         # Large object file support
    /utf-8          # Source and execution character set
    /Zc:__cplusplus # Report correct __cplusplus macro value
)

# ── Release optimisations ───────────────────────────────────────────────────
add_compile_options(
    $<$<CONFIG:Release>:/GL>   # Whole-program optimisation
    $<$<CONFIG:Release>:/Oi>   # Intrinsic functions
    $<$<CONFIG:Release>:/Gy>   # Function-level linking
)

add_link_options(
    $<$<CONFIG:Release>:/LTCG>         # Link-time code generation
    $<$<CONFIG:Release>:/OPT:REF>      # Eliminate unreferenced code
    $<$<CONFIG:Release>:/OPT:ICF>      # Identical COMDAT folding
)

# ── Unicode ─────────────────────────────────────────────────────────────────
add_compile_definitions(UNICODE _UNICODE)

# ── Debug / Release defines ────────────────────────────────────────────────
add_compile_definitions(
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

# ── Output directories (Visual Studio convention) ───────────────────────────
# Detect platform name for output paths
if(CMAKE_GENERATOR_PLATFORM)
    set(EARTHRISE_PLATFORM_NAME "${CMAKE_GENERATOR_PLATFORM}")
else()
    set(EARTHRISE_PLATFORM_NAME "x64")
endif()

# Final output (executables) → <root>/<Config>/<Platform>/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/$<CONFIG>/${EARTHRISE_PLATFORM_NAME}")
set(CMAKE_PDB_OUTPUT_DIRECTORY     "${CMAKE_SOURCE_DIR}/$<CONFIG>/${EARTHRISE_PLATFORM_NAME}")

# ── C++/WinRT (via vcpkg) ───────────────────────────────────────────────────
find_package(cppwinrt CONFIG REQUIRED)

# ── PIX profiler (optional, via vcpkg feature "pix") ───────────────────────
option(EARTHRISE_USE_PIX "Enable PIX GPU profiler support" OFF)

if(EARTHRISE_USE_PIX)
    find_package(WinPixEventRuntime CONFIG REQUIRED)
    message(STATUS "PIX profiler enabled via vcpkg")
endif()

# ── Sub-projects ────────────────────────────────────────────────────────────
add_subdirectory(NeuronCore)
add_subdirectory(NeuronClient)
add_subdirectory(NeuronServer)
add_subdirectory(Earthrise)
add_subdirectory(EarthRiseServer)
