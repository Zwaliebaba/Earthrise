# NeuronServer — server-side static library
add_library(NeuronServer STATIC
    pch.cpp
)

target_include_directories(NeuronServer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Per-module build output → <module>/<Config>/<Platform>/
set_target_properties(NeuronServer PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/$<CONFIG>/${EARTHRISE_PLATFORM_NAME}"
    COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/$<CONFIG>/${EARTHRISE_PLATFORM_NAME}"
)

# Precompiled header
target_precompile_headers(NeuronServer PRIVATE pch.h)

# Depends on NeuronCore
target_link_libraries(NeuronServer PUBLIC NeuronCore)

# Compile definitions
target_compile_definitions(NeuronServer PRIVATE _LIB)
