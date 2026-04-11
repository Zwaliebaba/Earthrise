# NeuronServer — server-side static library
add_library(NeuronServer STATIC
    BandwidthManager.cpp
    ClientSession.cpp
    SessionManager.cpp
    pch.cpp
)

target_include_directories(NeuronServer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Per-module build output → <module>/<Config>/<Platform>/
set_target_properties(NeuronServer PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${EARTHRISE_PLATFORM_NAME}/$<CONFIG>"
    COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${EARTHRISE_PLATFORM_NAME}/$<CONFIG>"
)

# Precompiled header
target_precompile_headers(NeuronServer PRIVATE pch.h)

# Depends on NeuronCore
target_link_libraries(NeuronServer PUBLIC NeuronCore)

# Compile definitions
target_compile_definitions(NeuronServer PRIVATE _LIB)
