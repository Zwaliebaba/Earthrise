# Celestial Bodies — Planets & Sun Implementation Plan

## Overview

Add two new `SpaceObjectCategory` values — `Planet` and `Sun` — as the first concrete step toward the procedurally generated Universe described in [Universe.md](Universe.md).

- **2 planets**: rendered with the same landscape-colored `SurfaceRenderer` pipeline as asteroids, using new dedicated planet meshes. Placed far enough to appear thousands of km away.
- **1 sun**: rendered as a camera-facing billboard textured with `Glow.dds`, with heavy bloom via the existing `PostProcess` system.

Both are **camera-locked** (translate with the camera so they never get closer or farther), giving the illusion of infinite distance — just like real celestial bodies in a space game. Sun locks at a slightly farther distance than planets to guarantee planets always occlude the sun.

---

## 1. Current State

| Area | Status |
|---|---|
| `SpaceObjectCategory` | 9 values (`SpaceObject`…`Turret`). No `Planet` or `Sun`. |
| Asteroid rendering | `SurfaceRenderer` — landscape-colored vertices baked from height/steepness gradient textures. 5 `SurfaceType` variants. 6 asteroid meshes (`Asteroid01`–`06.cmo`). |
| Textured 3D rendering | **None** — all objects use flat color or landscape color. No texture-mapped 3D pipeline. |
| `PostProcess` bloom | Fully implemented (extract → Gaussian blur → additive composite). **Not connected** — `ApplyBloom()` is never called. |
| `SpriteBatch` | Screen-space solid-color quads only. No texture support. |
| `Glow.dds` | Exists at `GameData/Textures/Glow.dds`. Used by nothing currently. |
| Far clip distance | 20 km (reverse-Z). |

---

## 2. Design Decisions

### 2.1 Camera-Locked Placement (Skybox Objects)

**Problem**: The zone is 100 km and the far clip is 20 km. Planets/suns placed at realistic distances would be culled or require extreme draw distances.

**Solution**: Camera-locked rendering — sun and planet positions are computed as `cameraPosition + fixedDirection * lockDistance` every frame. They move with the camera, so they always appear at the same direction and angular size. This is the standard technique for celestial bodies in space games.

- `planetLockDistance` = 18 000 m (just under the 20 km far clip)
- `sunLockDistance` = 19 000 m (behind planets — guarantees planets occlude the sun, avoids z-fighting)
- The world matrix uses this locked position instead of the "real" position
- After origin-rebasing: the rebased position becomes simply `fixedDirection * lockDistance`

Each celestial body stores a **fixed direction** (unit vector from zone center) and a **visual scale**. The server sends the real far-away position; the client overrides the render position using camera-locking.

### 2.2 Planet Rendering — Reuse SurfaceRenderer

Planets render exactly like asteroids:
1. Load a planet `.cmo` mesh from `GameData/Meshes/Planets/`
2. Bake landscape colors via `SurfaceRenderer::GetSurfaceMesh(meshKey, surfaceType)`
3. Render with `SurfaceRenderer::RenderObject()` in Pass 2 (asteroid/planet pass)

The same 5 `SurfaceType` variants apply — an `Earth` surface type planet looks like Earth, a `Desert` planet looks arid, etc. No shader changes needed.

### 2.3 Sun Rendering — New Billboard Pipeline

The sun is a camera-facing textured quad rendered in 3D space. This requires a small new rendering component:

```
SunBillboard (new, GameRender/)
├── Loads Glow.dds as a GPU texture (SRV)
├── Creates a simple textured-quad PSO (additive blended, no depth write)
├── Each frame: builds a camera-facing quad at the sun's camera-locked position
└── Outputs white (1.0) — bloom extracts via lowered threshold
```

**Shader**: Standard billboard vertex shader (expand 2 triangles from center point + camera right/up vectors) + pixel shader that samples `Glow.dds` and multiplies by sun base color (clamped to 1.0 by LDR back buffer).

**Blend state**: Additive — `SrcBlend=One, DestBlend=One`. The glow adds to whatever is behind it.

**Depth**: Write disabled, test enabled (sun behind a planet is occluded).

### 2.4 Sun-Only Bloom (LDR-Compatible)

**Constraint**: the back buffer is `B8G8R8A8_UNORM` (LDR, clamped to [0.0, 1.0]). The sun billboard cannot write HDR values (>1.0) — they are clamped by the render target before `ApplyBloom()` reads them.

**Solution**: Lower the bloom threshold to **0.65** so near-white pixels (the sun at luminance ≈ 0.95) are extracted. Mitigate bloom bleed onto other objects by keeping all non-sun entity colors below ~0.8 luminance (already the case — ships, stations, asteroids use muted colors).

This gives sun-dominant bloom with a small modification to the PostProcess composite step.

**Additive-only bloom composite**: The existing `PostProcess::ApplyBloom()` composite pass reads the scene as an SRV, which would require an intermediate scene RT (the back buffer can't be simultaneously SRV and RTV). Instead, add a new `m_compositeAdditivePSO` that uses **additive blending** (`SrcBlend=One, DestBlend=One`) and a simplified composite shader that only reads the `BloomTexture` — the bloom is added directly on top of the back buffer. The scene texture is never needed. This avoids allocating an intermediate RT entirely.

> **TODO (future)**: For proper HDR bloom, render the scene to an `R16G16B16A16_FLOAT` intermediate RT, run bloom on that, then resolve to the `B8G8R8A8` back buffer. This is the correct long-term approach but requires plumbing an intermediate RT through the full pipeline. At that point, restore the full scene+bloom composite pass.

**Render order**:
```
RenderScene()
  1. Clear RT + depth
  2. Starfield
  3. RenderWorldEntities()           // includes planets (landscape) + sun (billboard)
  4. Particles
  5. PostProcess::ApplyBloom()       // NEW — threshold=0.65, sun-dominant bloom
  6. Tactical grid
```

---

## 3. New & Modified Files

### 3.1 NeuronCore (shared types)

#### SpaceObjectCategory.h — add `Planet`, `Sun`

```cpp
enum class SpaceObjectCategory : uint8_t
{
    SpaceObject,
    Asteroid,
    Crate,
    Decoration,
    Ship,
    Jumpgate,
    Projectile,
    Station,
    Turret,
    Planet,     // NEW
    Sun,        // NEW
    COUNT
};
ENUM_HELPER(SpaceObjectCategory, SpaceObject, Sun)  // Update sentinel
```

#### PlanetData.h — new file

```cpp
#pragma once

namespace Neuron
{
    struct PlanetData
    {
        EntityHandle Owner;
        float        OrbitRadius    = 0.0f;   // Distance from parent sun (future: orbital mechanics)
        float        RotationSpeed  = 0.0f;
        XMFLOAT3     RotationAxis   = { 0.0f, 1.0f, 0.0f };
        bool         HasRings       = false;  // If true, a Ring cluster is parented to this planet
    };
}
```

Superset of the fields from [Universe.md](Universe.md) so the struct won't break when Universe generation is implemented. `OrbitRadius` and `HasRings` are unused for now — planets are decorative.

#### SunData.h — new file

```cpp
#pragma once

namespace Neuron
{
    struct SunData
    {
        EntityHandle Owner;
        float        Luminosity     = 1.0f;   // Future: heat damage zone radius
        float        VisualRadius   = 800.0f;  // Billboard half-size in world units
        float        RotationSpeed  = 0.0f;
        XMFLOAT3     RotationAxis   = { 0.0f, 1.0f, 0.0f };
    };
}
```

Superset of fields from [Universe.md](Universe.md) plus `VisualRadius` for billboard rendering. `Luminosity` replaces `Intensity` — the HDR multiplier is no longer needed since the back buffer is LDR; the sun just outputs white (1.0).

### 3.2 GameLogic (server-side)

#### SpaceObjectManager.h/.cpp — add component arrays

```cpp
// New parallel arrays (same pattern as existing categories)
std::vector<Neuron::PlanetData> m_planetData;
std::vector<Neuron::SunData>    m_sunData;

[[nodiscard]] PlanetData* GetPlanetData(EntityHandle _handle);
[[nodiscard]] SunData*    GetSunData(EntityHandle _handle);
```

`CreateEntity()` switch: initialize `PlanetData`/`SunData` at the slot for the new categories.

#### ZoneLoader.cpp — add test celestial bodies to `CreateTestZone()`

Place 2 planets and 1 sun at far positions:

```cpp
// Sun — center of the system, far above the action plane
// MeshHash = 0: sun is a billboard, no .cmo mesh. Client skips mesh lookup for Sun category.
auto sunHandle = manager.CreateEntity(SpaceObjectCategory::Sun,
    { 0.0f, 5000.0f, 40000.0f }, 0);
auto* sunObj = manager.GetSpaceObject(sunHandle);
sunObj->Color = { 1.0f, 0.95f, 0.8f, 1.0f };      // Warm white
sunObj->BoundingRadius = 500.0f;
auto* sunData = manager.GetSunData(sunHandle);
sunData->VisualRadius = 800.0f;

// Planet 1 — rocky/desert, upper-left of sky
auto planet1 = manager.CreateEntity(SpaceObjectCategory::Planet,
    { -30000.0f, 8000.0f, 25000.0f }, HashMeshName("Planet01"));
auto* p1Obj = manager.GetSpaceObject(planet1);
p1Obj->Color = { 0.8f, 0.6f, 0.4f, 1.0f };
p1Obj->BoundingRadius = 300.0f;

// Planet 2 — icy, lower-right of sky
auto planet2 = manager.CreateEntity(SpaceObjectCategory::Planet,
    { 20000.0f, -5000.0f, 35000.0f }, HashMeshName("Planet02"));
auto* p2Obj = manager.GetSpaceObject(planet2);
p2Obj->Color = { 0.5f, 0.7f, 0.9f, 1.0f };
p2Obj->BoundingRadius = 250.0f;
```

### 3.3 GameRender (client rendering)

#### MeshCache.cpp — add category folders

```cpp
case SpaceObjectCategory::Planet: return "Planets";
case SpaceObjectCategory::Sun:    return "Suns";
```

> **Note**: `CategoryRenderers.h` stubs are **not** added for `Planet`/`Sun`. The per-category renderer classes are currently unused in the render loop (`RenderWorldEntities()` dispatches directly). Adding empty stubs increases maintenance cost with no benefit. If the render loop is refactored to use `CategoryRenderer` dispatch in the future, add them then.

#### SunBillboard.h — new file

```cpp
#pragma once

namespace Neuron::Graphics
{
    // Renders the sun as a camera-facing textured quad.
    // Uses Glow.dds and additive blending. Outputs near-white pixels
    // that exceed the bloom threshold (0.65) for the sun glow effect.
    class SunBillboard
    {
    public:
        void Initialize();

        // Render a sun billboard at the given world position (already origin-rebased).
        void XM_CALLCONV Render(
            ID3D12GraphicsCommandList* _cmdList,
            ConstantBufferAllocator& _cbAlloc,
            ShaderVisibleHeap& _srvHeap,
            const Camera& _camera,
            FXMVECTOR _rebasedPosition,     // Origin-rebased sun position
            float _visualRadius,            // Half-size of the billboard quad
            FXMVECTOR _color);              // Base color (warm white; clamped to 1.0 by LDR RT)

    private:
        void CreatePSO();
        void LoadTexture();

        winrt::com_ptr<ID3D12PipelineState> m_pso;
        winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
        winrt::com_ptr<ID3D12Resource>      m_glowTexture;
        D3D12_GPU_DESCRIPTOR_HANDLE         m_glowSRV{};

        struct BillboardConstants
        {
            XMFLOAT4X4 ViewProjection;
            XMFLOAT3   CameraRight;  // Camera right vector (for billboard orientation)
            float      Radius;       // Half-size
            XMFLOAT3   CameraUp;     // Camera up vector
            float      _Pad0;
            XMFLOAT3   Center;       // Origin-rebased sun position
            float      _Pad1;
            XMFLOAT4   Color;        // RGB base color, alpha=1
        };
    };
}
```

#### SunBillboard.cpp — new file

**Vertex Shader** (inline HLSL):
```hlsl
cbuffer BillboardCB : register(b0)
{
    float4x4 ViewProjection;
    float3   CameraRight;
    float    Radius;
    float3   CameraUp;
    float    _Pad0;
    float3   Center;
    float    _Pad1;
    float4   Color;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

// Expand 6 vertices into a camera-facing quad (2 triangles).
static const float2 QuadUVs[6] = {
    {0,0}, {1,0}, {0,1},
    {1,0}, {1,1}, {0,1}
};
static const float2 QuadOffsets[6] = {
    {-1,+1}, {+1,+1}, {-1,-1},
    {+1,+1}, {+1,-1}, {-1,-1}
};

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;
    float2 offset = QuadOffsets[vertexId] * Radius;
    output.TexCoord = QuadUVs[vertexId];

    // Build camera-facing quad using right/up vectors from constant buffer
    float3 worldPos = Center
        + CameraRight * offset.x
        + CameraUp    * offset.y;

    output.Position = mul(float4(worldPos, 1.0), ViewProjection);
    return output;
}
```

**Pixel Shader** (inline HLSL):
```hlsl
Texture2D GlowTexture : register(t0);
SamplerState LinearSampler : register(s0);

cbuffer BillboardCB : register(b0)
{
    float4x4 ViewProjection;
    float3   CameraRight;
    float    Radius;
    float3   CameraUp;
    float    _Pad0;
    float3   Center;
    float    _Pad1;
    float4   Color;   // .rgb = base color (LDR, clamped to 1.0), .a = 1
};

float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float4 texel = GlowTexture.Sample(LinearSampler, uv);
    return float4(texel.rgb * Color.rgb, texel.a * Color.a);
}
```

**PSO Configuration**:
- Blend: Additive (`SRC_COLOR=One, DST_COLOR=One`, `SRC_ALPHA=One, DST_ALPHA=One`)
- Depth write: **disabled** (sun is a transparent effect)
- Depth test: **enabled** (`GREATER_EQUAL`, reverse-Z) — sun behind a station is occluded
- Cull mode: None (billboard has no back face)
- Input layout: None (vertex-ID driven)
- Root signature: 1 CBV (BillboardConstants) + 1 SRV (Glow texture) + 1 static sampler

**Texture Loading**:
- `Glow.dds` loaded via `GpuResourceManager` (or a direct `CreateDDSTextureFromFile` call matching existing patterns)
- SRV created in the `ShaderVisibleHeap`

### 3.4 Client (Earthrise/)

#### ClientWorldState — handle new categories

The existing spawn handler resolves `MeshHash` → `MeshKey` using `MeshCache::BuildMeshHashMap()`. Add `Planet` and `Sun` to the category list so mesh hashes resolve correctly.

For planets, use the existing `SpaceObject::Surface` field (already in `SpaceObject` struct) — the server sets it when creating the planet, and the client reads it from the spawn message. **Verify** that `StateBroadcaster` / `EntitySpawnMsg` serializes the `Surface` field; if not, add it.

Sun rendering parameters (`VisualRadius`) are hardcoded on the client for now — only 1 sun exists and these are visual-only values not worth extending the network protocol for.

#### GameApp — render pipeline integration

```
RenderWorldEntities() — updated:
  ─── PASS 1: NON-ASTEROIDS, NON-PLANETS, NON-SUNS (FlatColorPipeline) ───
  ... existing code, add: skip Planet and Sun ...

  ─── PASS 2: ASTEROIDS + PLANETS (SurfaceRenderer / VertexColorPipeline) ───
  ForEachActive where Category == Asteroid or Planet:
    if (Category == Planet)
      compute camera-locked position: dir * LOCK_DISTANCE
    apply SurfaceRenderer as before

  ─── PASS 3: SUN BILLBOARD (NEW) ───
  ForEachActive where Category == Sun:
    compute camera-locked position: dir * SUN_LOCK_DISTANCE
    frustum-cull the locked position
    m_sunBillboard.Render(cmdList, cbAlloc, srvHeap, camera,
        lockedPos, SUN_VISUAL_RADIUS, sunColor);

RenderScene() — updated:
  1. Clear
  2. Starfield
  3. RenderWorldEntities()    // now includes planets + sun
  4. Particles
  5. PostProcess::ApplyBloom(threshold=0.65)  // NEW — sun-dominant bloom (LDR)
  6. Tactical grid
```

#### Camera-Lock Helper

```cpp
// Compute the origin-rebased locked position for a skybox-style celestial body.
// The body always appears at a fixed direction and angular size regardless of camera position.
// Returns a zero vector if the body is at the camera position (degenerate case).
inline XMVECTOR XM_CALLCONV ComputeCameraLockedPosition(
    FXMVECTOR _bodyWorldPos,
    FXMVECTOR _cameraWorldPos,
    float _lockDistance)  // PLANET_LOCK_DISTANCE or SUN_LOCK_DISTANCE
{
    XMVECTOR diff = XMVectorSubtract(_bodyWorldPos, _cameraWorldPos);
    float len = XMVectorGetX(XMVector3Length(diff));
    if (len < 1.0f)
        return XMVectorSet(0.0f, 0.0f, _lockDistance, 0.0f); // Fallback: straight ahead
    XMVECTOR dir = XMVectorScale(diff, 1.0f / len);
    // Rebased position = direction * lock distance (camera is at origin after rebasing)
    return XMVectorScale(dir, _lockDistance);
}
```

The visual scale of the mesh must be adjusted to account for the compressed distance. If a planet is "really" at 40 km but rendered at 18 km, it needs to be scaled by `18/40 = 0.45` to maintain the same angular size. The mesh's modeled size must first be scaled by `BoundingRadius` (meshes are roughly unit-scale), then by the distance compression factor:

```cpp
float realDistance = XMVectorGetX(XMVector3Length(XMVectorSubtract(bodyWorldPos, cameraPos)));
float distScale = PLANET_LOCK_DISTANCE / realDistance;
float baseScale = spaceObj->BoundingRadius;  // mesh is ~unit, scale to real size
float finalScale = baseScale * distScale;
XMMATRIX world = XMMatrixScaling(finalScale, finalScale, finalScale) * rotation * translation;
```

> **Frustum culling**: Apply `Camera::IsVisible()` to the locked position before rendering. The current `RenderWorldEntities()` does not frustum-cull, but celestial bodies behind the camera should be skipped to avoid unnecessary draw calls.

---

## 4. New Mesh Assets

### 4.1 Planet Meshes

Create `GameData/Meshes/Planets/`:
- `Planet01.cmo` — rocky/cratered sphere with significant vertex displacement (craters, mountains)
- `Planet02.cmo` — icy sphere with ridges and surface variation

**Important**: Meshes must be **bumpy/displaced spheres**, not smooth spheres. `SurfaceRenderer` colors vertices based on height (Y coordinate) and normal steepness. On a smooth sphere, both Y and Normal.Y are pure functions of latitude, producing horizontal color bands with no longitudinal variation. Vertex displacement creates local height/steepness variation that breaks the banding and produces visually interesting landscape coloring.

Poly budget: ~1500–2000 triangles per planet mesh — enough geometry for meaningful surface displacement.

> **Visual risk — Darwinia edge lines**: The `VertexColorPipeline` renders wireframe edges via screen-space face-normal discontinuity. On a small on-screen sphere (planet at distance), edge lines may dominate, making the planet look like a wireframe ball. Test visually; if needed, suppress edge detection for `Planet` category by increasing the edge threshold or bypassing the edge kernel.

### 4.2 Sun Meshes

No mesh needed — the sun is a billboard quad generated from vertex IDs. No `GameData/Meshes/Suns/` directory is created. The sun is created with `MeshHash = 0` (no mesh); the client skips mesh lookup for `Sun`-category entities.

---

## 5. Implementation Steps

### Phase 1 — Types & Server (no rendering yet)

| Step | Files | Description |
|---|---|---|
| 1.1 | `NeuronCore/GameTypes/SpaceObjectCategory.h` | Add `Planet`, `Sun` to enum; update `ENUM_HELPER` sentinel |
| 1.2 | `NeuronCore/GameTypes/PlanetData.h` | New file — `PlanetData` struct |
| 1.3 | `NeuronCore/GameTypes/SunData.h` | New file — `SunData` struct |
| 1.4 | `GameLogic/SpaceObjectManager.h/.cpp` | Add `m_planetData`, `m_sunData` vectors + accessors; update `CreateEntity` switch (NOT `DestroyEntity` — it has no per-category cleanup and neither `PlanetData` nor `SunData` hold resources) |
| 1.5 | `GameRender/MeshCache.cpp` | Add `Planet`→`"Planets"`, `Sun`→`"Suns"` to `GetCategoryFolder()` |
| 1.6 | `NeuronCore/CMakeLists.txt` | Add `PlanetData.h`, `SunData.h` to sources (if explicit file lists, not globbing) |
| 1.7 | Fix all `switch(category)` | Compiler warnings/errors will flag every unhandled `Planet`/`Sun` case — fix each one (typically `default:` or explicit no-op) |
| 1.8 | ~~Verify `EntitySpawnMsg` serialization~~ | **Verified** — `EntitySpawnMsg` already serializes `SurfaceType`. `StateBroadcaster::SendInitialSpawns` populates `spawn.Surface = _obj.Surface`. No changes needed. |
| 1.9 | Build both configs | `cmake --build --preset x64-debug` and `x64-release` — must compile clean |

### Phase 2 — Planet Rendering

| Step | Files | Description |
|---|---|---|
| 2.1 | `GameData/Meshes/Planets/Planet01.cmo`, `Planet02.cmo` | Create planet sphere meshes (export from modeling tool) |
| 2.2 | `Earthrise/ClientWorldState.h/.cpp` | Handle `Planet` category in spawn processing; assign `SurfaceType` |
| 2.3 | `Earthrise/GameApp.cpp` — `RenderWorldEntities()` | Add planets to Pass 2 (SurfaceRenderer pass) with camera-locked position and distance-scaled world matrix |
| 2.4 | `EarthRiseServer/ZoneLoader.cpp` | Add 2 planets to `CreateTestZone()` |
| 2.5 | Build + visual test | Verify planets appear as landscape-colored spheres in the far sky |

### Phase 3 — Sun Billboard + Bloom

| Step | Files | Description |
|---|---|---|
| 3.1 | `GameRender/SunBillboard.h` | New file — `SunBillboard` class declaration |
| 3.2 | `GameRender/SunBillboard.cpp` | New file — PSO creation, Glow.dds loading, billboard rendering with inline HLSL vertex+pixel shaders |
| 3.3 | `GameRender/CMakeLists.txt` | Add `SunBillboard.cpp/.h` |
| 3.4 | `Earthrise/GameApp.h` | Add `SunBillboard m_sunBillboard` member |
| 3.5 | `Earthrise/GameApp.cpp` | Initialize `m_sunBillboard`; add Pass 3 (sun billboard) to `RenderWorldEntities()` with camera-locked position |
| 3.6 | `GameRender/PostProcess.cpp` | Add additive-only bloom composite: new `m_compositeAdditivePSO` with additive blend + simplified shader that only reads `BloomTexture`. Add `ApplyBloomAdditive()` method that uses this PSO (skips scene RT read). |
| 3.7 | `Earthrise/GameApp.cpp` | Wire `ApplyBloomAdditive(threshold=0.65)` call in `RenderScene()` after particles, before tactical grid. |
| 3.8 | `Earthrise/GameApp.h/.cpp` | Override `OnWindowSizeChanged()` — forward to `m_postProcess.OnResize()` (confirmed missing from `GameApp`; base class does NOT call it). |
| 3.9 | `EarthRiseServer/ZoneLoader.cpp` | Add 1 sun to `CreateTestZone()` |
| 3.10 | Build + visual test | Verify sun appears as a glowing billboard with bloom halo; other objects do not bloom |

### Phase 4 — Unit Tests

| Step | Files | Description |
|---|---|---|
| 4.1 | `EarthRiseTests/EntityPoolTests.cpp` | Test `CreateEntity` / `DestroyEntity` with `Planet` and `Sun` categories; verify `GetPlanetData()` / `GetSunData()` return valid pointers |
| 4.2 | `EarthRiseTests/SerializationTests.cpp` | Test `EntitySpawnMsg` round-trip for `Planet` and `Sun` categories |
| 4.3 | `EarthRiseTests/RenderMathTests.cpp` | Test `ComputeCameraLockedPosition` — normal case, zero-length (degenerate), behind-camera direction |

---

## 6. Rendering Pipeline — Before & After

### Before

```
RenderScene()
  1. Clear RT + depth (0.0 reverse-Z)
  2. Starfield
  3. RenderWorldEntities()
     │  Pass 1: FlatColorPipeline (ships, stations, jumpgates, crates, turrets, projectiles)
     │  Pass 2: SurfaceRenderer   (asteroids only)
  4. Particles
  5. Tactical grid
  ──────────────
  RenderCanvas() → UI overlay
```

### After

```
RenderScene()
  1. Clear RT + depth (0.0 reverse-Z)
  2. Starfield
  3. RenderWorldEntities()
     │  Pass 1: FlatColorPipeline (ships, stations, jumpgates, crates, turrets, projectiles, decorations)
     │  Pass 2: SurfaceRenderer   (asteroids + PLANETS — camera-locked, distance-scaled, frustum-culled)
     │  Pass 3: SunBillboard      (SUN — camera-locked at 19 km, Glow.dds textured, additive, LDR white)
  4. Particles
  5. PostProcess::ApplyBloom(threshold=0.65)   ← NEW (LDR-compatible, sun-dominant)
  6. Tactical grid
  ──────────────
  RenderCanvas() → UI overlay
```

---

## 7. Constants & Tuning

| Parameter | Value | Location | Notes |
|---|---|---|---|
| `PLANET_LOCK_DISTANCE` | 18 000 m | `GameApp.cpp` or `Camera.h` | Must be < far clip (20 000 m) |
| `SUN_LOCK_DISTANCE` | 19 000 m | `GameApp.cpp` or `Camera.h` | Behind planets — prevents z-fighting, ensures planet occlusion |
| Bloom threshold | 0.65 | `GameApp.cpp` → `ApplyBloom()` call | LDR-compatible; extracts near-white pixels (sun). Keep non-sun colors below ~0.8 luminance |
| Sun billboard radius | 800 m (at lock distance) | Client hardcoded | Adjust for desired angular size |
| Planet scale | Auto-computed | `finalScale = BoundingRadius * (PLANET_LOCK_DISTANCE / realDistance)` | Preserves angular size at lock distance |
| Planet 1 position | (-30000, 8000, 25000) | `ZoneLoader.cpp` | Upper-left sky |
| Planet 2 position | (20000, -5000, 35000) | `ZoneLoader.cpp` | Lower-right sky |
| Sun position | (0, 5000, 40000) | `ZoneLoader.cpp` | Center-above |

---

## 8. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| `SurfaceRenderer` latitude banding on smooth sphere | Planets look flat / horizontal bands | Use bumpy/displaced sphere meshes (craters, mountains); vertex displacement breaks the banding |
| Darwinia edge lines dominate small on-screen planet | Planet looks like wireframe ball | Test visually; suppress edge detection for `Planet` category if needed (increase edge threshold) |
| Camera-lock degenerate case (body at camera pos) | `XMVector3Normalize` on zero vector → NaN | Guard with length check; fallback to straight-ahead direction |
| Camera-lock body behind camera | Invisible but draw call wasted | Apply `Camera::IsVisible()` frustum cull to locked position before rendering |
| LDR back buffer: bloom threshold too low | Bright non-sun objects (white ships) bloom | Keep entity colors below ~0.8 luminance; threshold 0.65 provides margin. TODO: HDR RT later |
| Bloom bleeds onto nearby UI | Unwanted glow on HUD | Bloom runs before `RenderCanvas()`; safe |
| PostProcess scene RT conflict | Full composite pass reads scene as SRV while writing back buffer — impossible without intermediate RT | Use additive-only bloom composite (`ApplyBloomAdditive`): only reads bloom RT, additively blends onto back buffer. No scene SRV needed. |
| Z-fighting between sun and planet at same lock distance | Depth artifacts | Sun locks at 19 km, planets at 18 km — different depth values |
| `PostProcess::OnResize()` not called on window resize | Bloom breaks after resize | Override `OnWindowSizeChanged()` in `GameApp` and forward to `m_postProcess.OnResize()` (Phase 3.8; confirmed missing) |
| Planet mesh assets missing at first | Crash or invisible planets | `MeshCache::GetMesh()` already returns `nullptr` for missing files; rendering gracefully skips nulls |
| Many `switch(category)` need updating | Missed cases → warnings or UB | Build with `/W4` — compiler flags unhandled enum cases. Fix exhaustively in Phase 1 |
| ~~`EntitySpawnMsg` may not serialize `SurfaceType`~~ | ~~Planets use wrong SurfaceType on client~~ | **Verified**: Already serialized. No risk. |
| Particles also bloom | Engine exhaust / weapon flashes above threshold 0.65 will glow | May be desirable; if not, lower particle brightness or move bloom before particles |
| AoI filtering drops celestial bodies from state snapshots | Planets/suns at 30–40 km are outside `DEFAULT_AOI_RADIUS` (10 km) | Safe — bodies don't move, so initial spawn position is sufficient. If orbital motion is added later, exempt celestial bodies from AoI or increase radius |

---

## 9. Future Extensions

- **Planet rotation animation**: Tick `PlanetData::RotationSpeed` in `MovementSystem` (or a new `CelestialBodySystem`). The self-rotation quaternion updates each frame.
- **Sun light direction**: Derive the global light direction from the sun's position relative to the camera. Currently hardcoded in `Camera::m_lightDirection`.
- **Multiple suns**: The billboard system already supports N suns. Bloom handles multiple sources naturally.
- **Planet rings**: Asteroid ring clusters parented to a planet, per [Universe.md](Universe.md).
- **Atmospheric halo**: Second billboard layer behind the planet mesh with a soft glow texture (same Glow.dds, tinted blue/orange).
- **Orbital motion**: Planets slowly orbit the sun with long periods (purely visual, no gameplay impact initially).
- **HDR render target**: Render the scene to `R16G16B16A16_FLOAT`, run bloom there, then resolve to `B8G8R8A8`. Enables true HDR bloom (sun intensity > 1.0), weapon glow, engine trails, etc.
- **Per-body lock distance**: Instead of fixed constants, compute `lockDist = min(realDistance * 0.45, 19000)` per body. Farther bodies appear smaller — adds depth stratification to the sky.
- **Server persistence**: `PlanetDef`, `SunDef` in `ObjectDefs.h` + `ObjectDefsLoader`; `ZoneLoader::LoadFromFile()` binary format support. Deferred to [Universe.md](Universe.md) implementation.
