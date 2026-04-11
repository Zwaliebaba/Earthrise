# NeuronClient — graphics / audio static library (client only)
add_library(NeuronClient STATIC
    NeuronClient.cpp
    pch.cpp
    Audio/AudioEngine.cpp
    Audio/SoundCommon.cpp
    Audio/SoundEffect.cpp
    Audio/SoundEffectInstance.cpp
    Audio/WAVFileReader.cpp
    Graphics/DescriptorHeap.cpp
    Graphics/GraphicsCore.cpp
)

target_include_directories(NeuronClient PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Audio
    ${CMAKE_CURRENT_SOURCE_DIR}/Graphics
)

# Per-module build output → <module>/<Config>/<Platform>/
set_target_properties(NeuronClient PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${EARTHRISE_PLATFORM_NAME}/$<CONFIG>"
    COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${EARTHRISE_PLATFORM_NAME}/$<CONFIG>"
)

# Precompiled header
target_precompile_headers(NeuronClient PRIVATE pch.h)

# Depends on NeuronCore (PUBLIC so consumers also get the include path)
target_link_libraries(NeuronClient PUBLIC NeuronCore)

# Libraries consumed via #pragma comment(lib,...) in headers:
#   dxgi.lib, dxguid.lib  — DirectXHelper.h
#   xaudio2.lib           — Audio.h
#   Gdi32.lib, Shell32.lib — NeuronClient.h
# Explicitly listed here for transparency.
target_link_libraries(NeuronClient PUBLIC
    d3d12.lib
    dxgi.lib
    dxguid.lib
    xaudio2.lib
    Gdi32.lib
    Shell32.lib
)

# Compile definitions
target_compile_definitions(NeuronClient PRIVATE _LIB)

# PIX profiler (optional, via vcpkg)
if(EARTHRISE_USE_PIX)
    target_link_libraries(NeuronClient PUBLIC Microsoft::WinPixEventRuntime)
    target_compile_definitions(NeuronClient PUBLIC PROFILER_ENABLED)
endif()
