# GameLogic — server-only simulation static library
add_library(GameLogic STATIC
    pch.cpp
    CollisionSystem.cpp
    CombatSystem.cpp
    CommandProcessor.cpp
    DockingSystem.cpp
    EntityPool.cpp
    FleetSystem.cpp
    JumpgateSystem.cpp
    LootSystem.cpp
    MovementSystem.cpp
    NpcAI.cpp
    ObjectDefsLoader.cpp
    ProjectileSystem.cpp
    SpaceObjectManager.cpp
    SpatialHash.cpp
)

target_include_directories(GameLogic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Per-module build output → <module>/<Config>/<Platform>/
set_target_properties(GameLogic PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${EARTHRISE_PLATFORM_NAME}/$<CONFIG>"
    COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${EARTHRISE_PLATFORM_NAME}/$<CONFIG>"
)

# Precompiled header
target_precompile_headers(GameLogic PRIVATE pch.h)

# Depends on NeuronCore (server-safe subset)
target_link_libraries(GameLogic PUBLIC NeuronCore)

# Compile definitions
target_compile_definitions(GameLogic PRIVATE _LIB)
