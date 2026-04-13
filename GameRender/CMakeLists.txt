# GameRender — rendering static library (client only)
add_library(GameRender STATIC
    pch.cpp
    BitmapFont.cpp
    Camera.cpp
    CmoLoader.cpp
    DdsLoader.cpp
    FlatColorPipeline.cpp
    GpuResourceManager.cpp
    LandscapeTexture.cpp
    Mesh.cpp
    MeshCache.cpp
    PipelineHelpers.cpp
    PostProcess.cpp
    ShaderVisibleHeap.cpp
    SpaceObjectRenderer.cpp
    SpriteBatch.cpp
    Starfield.cpp
    SunBillboard.cpp
    SurfaceRenderer.cpp
    TacticalGrid.cpp
    UploadHeap.cpp
    VertexColorPipeline.cpp
    ParticleSystem.cpp
    ParticleRenderer.cpp
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

# ── Offline shader compilation (DXC → header files) ─────────────────────────
target_compile_shaders(GameRender
    SHADER_MODEL 6_7
    SHADERS
        Shaders/FlatColorVS.hlsl
        Shaders/FlatColorPS.hlsl
        Shaders/VertexColorVS.hlsl
        Shaders/VertexColorPS.hlsl
        Shaders/FullscreenVS.hlsl
        Shaders/BloomExtractPS.hlsl
        Shaders/BloomBlurPS.hlsl
        Shaders/BloomCompositePS.hlsl
        Shaders/BloomAdditivePS.hlsl
        Shaders/SpriteVS.hlsl
        Shaders/SpritePS.hlsl
        Shaders/StarfieldVS.hlsl
        Shaders/StarfieldPS.hlsl
        Shaders/GridVS.hlsl
        Shaders/GridPS.hlsl
        Shaders/FontVS.hlsl
        Shaders/FontPS.hlsl
        Shaders/BillboardVS.hlsl
        Shaders/BillboardPS.hlsl
)
