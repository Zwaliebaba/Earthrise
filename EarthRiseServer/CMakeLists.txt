# EarthRiseServer — console server executable
add_executable(EarthRiseServer
    WinMain.cpp
    pch.cpp
)

target_include_directories(EarthRiseServer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Precompiled header
target_precompile_headers(EarthRiseServer PRIVATE pch.h)

# Dependencies
target_link_libraries(EarthRiseServer PRIVATE NeuronServer NeuronCore GameLogic)

# Final output → <root>/<Config>/<Platform>/
set_target_properties(EarthRiseServer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/${EARTHRISE_PLATFORM_NAME}/$<CONFIG>"
    PDB_OUTPUT_DIRECTORY     "${CMAKE_SOURCE_DIR}/${EARTHRISE_PLATFORM_NAME}/$<CONFIG>"
)

# ── Deploy GameData next to the executable ─────────────────────────────────
set(GAMEDATA_SOURCE_DIR "${CMAKE_SOURCE_DIR}/GameData")

if(EXISTS "${GAMEDATA_SOURCE_DIR}")
    add_custom_command(TARGET EarthRiseServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${GAMEDATA_SOURCE_DIR}"
            "$<TARGET_FILE_DIR:EarthRiseServer>/GameData"
        COMMENT "Copying GameData..."
    )
endif()
