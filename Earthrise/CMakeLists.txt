# Earthrise — Win32 desktop client executable
add_executable(Earthrise WIN32
    GameApp.cpp
    ServerConnection.cpp
    ClientWorldState.cpp
    Prediction.cpp
    InputState.cpp
    InputHandler.cpp
    FleetSelectionUI.cpp
    CommandTargeting.cpp
    HUD.cpp
    TargetingUI.cpp
    ChatUI.cpp
    FleetPanel.cpp
    JumpgateUI.cpp
    WinMain.cpp
    pch.cpp
    Earthrise.rc        # embeds app.manifest
)

target_include_directories(Earthrise PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Precompiled header
target_precompile_headers(Earthrise PRIVATE pch.h)

# Dependencies
target_link_libraries(Earthrise PRIVATE NeuronClient NeuronCore GameRender)

# Disable CMake's own manifest handling — manifest is embedded via Earthrise.rc
set_target_properties(Earthrise PROPERTIES
    LINK_FLAGS "/MANIFEST:NO"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/${EARTHRISE_PLATFORM_NAME}/$<CONFIG>"
    PDB_OUTPUT_DIRECTORY     "${CMAKE_SOURCE_DIR}/${EARTHRISE_PLATFORM_NAME}/$<CONFIG>"
)

# Ensure the resource compiler can find app.manifest relative to the .rc file
set_source_files_properties(Earthrise.rc PROPERTIES
    COMPILE_FLAGS "/I\"${CMAKE_CURRENT_SOURCE_DIR}\""
)


# ── Deploy mesh assets next to the executable ─────────────────────────────
set(MESH_SOURCE_DIR "${CMAKE_SOURCE_DIR}/GameData/Meshes")

if(EXISTS "${MESH_SOURCE_DIR}")
    add_custom_command(TARGET Earthrise POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${MESH_SOURCE_DIR}"
            "$<TARGET_FILE_DIR:Earthrise>/GameData/Meshes"
        COMMENT "Copying mesh assets to GameData/Meshes..."
    )
endif()
