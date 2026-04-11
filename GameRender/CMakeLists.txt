# GameRender — rendering static library (client only)
add_library(GameRender STATIC
    pch.cpp
)

target_include_directories(GameRender PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Per-module build output → <module>/<Config>/<Platform>/
set_target_properties(GameRender PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${EARTHRISE_PLATFORM_NAME}/$<CONFIG>"
    COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${EARTHRISE_PLATFORM_NAME}/$<CONFIG>"
)

# Precompiled header
target_precompile_headers(GameRender PRIVATE pch.h)

# Depends on NeuronClient and NeuronCore (PUBLIC so consumers also get the include paths)
target_link_libraries(GameRender PUBLIC NeuronClient NeuronCore)

# Compile definitions
target_compile_definitions(GameRender PRIVATE _LIB)
