# NeuronCore — shared static library (client + server)
add_library(NeuronCore STATIC
    EventManager.cpp
    FileSys.cpp
    NeuronCore.cpp
    TasksCore.cpp
    pch.cpp
)

target_include_directories(NeuronCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/GameMath
    ${CMAKE_CURRENT_SOURCE_DIR}/GameTypes
)

# Per-module build output → <module>/<Config>/<Platform>/
set_target_properties(NeuronCore PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${EARTHRISE_PLATFORM_NAME}/$<CONFIG>"
    COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${EARTHRISE_PLATFORM_NAME}/$<CONFIG>"
)

# Precompiled header
target_precompile_headers(NeuronCore PRIVATE pch.h)

# Libraries consumed via #pragma comment(lib,...) in headers:
#   Ws2_32.lib   — NeuronCore.h
#   OneCore.lib  — NeuronCore.h
# Explicitly listed here for transparency; MSVC honours the pragmas too.
target_link_libraries(NeuronCore PUBLIC
    Ws2_32.lib
    OneCore.lib
    Microsoft::CppWinRT  # C++/WinRT projections (via vcpkg)
)
