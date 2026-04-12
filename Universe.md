# Universe — Procedural Generation Architecture

## Overview

The **Universe** is the server-side world model representing a single, large procedurally generated star system. It contains suns, planets, and asteroid clusters — some shaped as orbital rings (like Saturn's rings), others as irregular supernova debris clouds. Asteroids contain minable resources, can be destroyed by weapon fire, and regrow stochastically within their parent cluster. The full universe state persists across server restarts.

The universe lives inside the existing `Zone` (100 km³). A seed drives deterministic placement of all celestial bodies and clusters; a delta file captures runtime mutations (depleted asteroids, regrowth timers).

---

## 1. Requirements

| Requirement | Detail |
|---|---|
| Zone topology | Single continuous zone (100 km), no multi-zone jumps required for access |
| Celestial bodies | 1–3 suns, 3–8 planets — decorative now, gameplay hooks later (gravity, heat) |
| Asteroid clusters | 10–30 clusters, 50–200 asteroids each, ≈3 000 total |
| Cluster shapes | **Ring** (torus around a parent body) and **Supernova** (irregular spherical cloud) |
| Mining | Weapon-based — mining laser damages asteroid, spawns resource crates |
| Regrowth | Depleted asteroids respawn at a random position within their parent cluster |
| Persistence | Seed + delta state saved; universe survives server restarts |
| Entity budget | ≤16 384 (current `EntityPool` cap) — 3 000 asteroids + ~100 other entities is safe |

---

## 2. New & Modified Types

### 2.1 SpaceObjectCategory additions

Add two new categories to the existing enum:

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
```

`ENUM_HELPER` sentinel updates from `Turret` → `Sun`.

### 2.2 PlanetData (NeuronCore/GameTypes/PlanetData.h)

```cpp
struct PlanetData
{
    EntityHandle Owner;
    float        OrbitRadius    = 0.0f;   // Distance from parent sun (future: orbital mechanics)
    float        RotationSpeed  = 0.0f;   // Visual self-rotation
    XMFLOAT3     RotationAxis   = { 0.0f, 1.0f, 0.0f };
    bool         HasRings       = false;  // If true, a Ring cluster is parented to this planet
};
```

### 2.3 SunData (NeuronCore/GameTypes/SunData.h)

```cpp
struct SunData
{
    EntityHandle Owner;
    float        Luminosity     = 1.0f;   // Future: heat damage zone radius
    float        RotationSpeed  = 0.0f;
    XMFLOAT3     RotationAxis   = { 0.0f, 1.0f, 0.0f };
};
```

### 2.4 AsteroidData — extend existing

Add a cluster ID so the regrowth system knows which cluster owns the asteroid:

```cpp
struct AsteroidData
{
    EntityHandle Owner;
    float        RotationSpeed  = 0.0f;
    XMFLOAT3     RotationAxis   = { 0.0f, 1.0f, 0.0f };
    uint32_t     ResourceType   = 0;
    float        ResourceAmount = 0.0f;
    uint16_t     ClusterId      = 0;         // NEW — parent cluster index
    float        MaxResource    = 0.0f;      // NEW — initial resource cap (for regrowth)
};
```

### 2.5 AsteroidCluster (new struct, GameLogic)

Describes the procedural shape and resource profile of one cluster. Not an entity — metadata owned by `UniverseGenerator`.

```cpp
enum class ClusterShape : uint8_t
{
    Ring,       // Torus around a parent celestial body
    Supernova,  // Irregular spherical debris cloud
    COUNT
};

struct AsteroidCluster
{
    uint16_t      ClusterId       = 0;
    ClusterShape  Shape           = ClusterShape::Supernova;

    // Spatial definition
    XMFLOAT3      Center          = {};
    XMFLOAT4      Orientation     = { 0, 0, 0, 1 }; // Tilt of the ring plane
    float         InnerRadius     = 0.0f;            // Ring: hole radius; Supernova: min spawn radius
    float         OuterRadius     = 0.0f;            // Outer extent
    float         Thickness       = 0.0f;            // Ring: vertical thickness; Supernova: unused

    // Content
    uint32_t      AsteroidCount   = 0;               // Target count when fully populated
    uint32_t      ResourceType    = 0;
    float         MinResource     = 0.0f;
    float         MaxResource     = 0.0f;
    float         MinRadius       = 0.5f;             // Bounding radius range for asteroids
    float         MaxRadius       = 3.0f;

    // Regrowth
    float         RespawnInterval = 300.0f;           // Seconds before a depleted slot regrows
    EntityHandle  ParentBody;                          // Planet or Sun this cluster orbits (Invalid = free-floating)

    // Runtime bookkeeping (not serialized in seed — stored in delta)
    uint32_t      ActiveCount     = 0;
    struct RegrowthEntry
    {
        float     Timer = 0.0f;                       // Seconds remaining
    };
    std::vector<RegrowthEntry> RegrowthQueue;
};
```

---

## 3. New Systems

### 3.1 UniverseGenerator (GameLogic)

**Files**: `UniverseGenerator.h`, `UniverseGenerator.cpp`

Deterministic, seed-based procedural generation. Given the same seed, produces the same universe layout every time. Uses a `std::mt19937` PRNG seeded once at generation time.

```
┌─────────────────────────────────────────────────────┐
│                 UniverseGenerator                    │
│                                                      │
│  Input:  uint64_t seed                               │
│  Output: UniverseLayout (suns, planets, clusters)    │
│                                                      │
│  1. Place suns (1-3) near zone center                │
│  2. Place planets (3-8) at orbital distances          │
│  3. Generate ring clusters around planets w/ rings    │
│  4. Generate supernova clusters at random positions   │
│  5. Populate each cluster with individual asteroids   │
└─────────────────────────────────────────────────────┘
```

#### Key API

```cpp
class UniverseGenerator
{
public:
    struct UniverseLayout
    {
        struct CelestialBody
        {
            SpaceObjectCategory Category;   // Planet or Sun
            XMFLOAT3            Position;
            float               BoundingRadius;
            XMFLOAT4            Color;
            uint32_t            MeshNameHash;
            // Category-specific init data
            PlanetData          PlanetInit;
            SunData             SunInit;
        };

        struct AsteroidSpawn
        {
            XMFLOAT3  Position;
            float     BoundingRadius;
            XMFLOAT4  Color;
            uint32_t  MeshNameHash;
            float     RotationSpeed;
            XMFLOAT3  RotationAxis;
            uint32_t  ResourceType;
            float     ResourceAmount;
            uint16_t  ClusterId;
        };

        std::vector<CelestialBody>   Bodies;
        std::vector<AsteroidCluster> Clusters;
        std::vector<AsteroidSpawn>   Asteroids;
    };

    [[nodiscard]] static UniverseLayout Generate(uint64_t _seed);

private:
    // Placement helpers
    static XMFLOAT3 RandomPointInTorus(std::mt19937& _rng,
        FXMVECTOR _center, FXMVECTOR _orientation,
        float _innerR, float _outerR, float _thickness);

    static XMFLOAT3 RandomPointInSupernovaCloud(std::mt19937& _rng,
        FXMVECTOR _center, float _innerR, float _outerR);
};
```

#### Generation Algorithm Detail

**Suns** — placed within ±5 km of zone center. Large bounding radius (50–200 m game units). Bright color.

**Planets** — placed on concentric orbital shells around the primary sun (10–40 km). Each planet has a random tilt. Some planets are flagged `HasRings = true`.

**Ring Clusters** — for each planet with `HasRings`:
- Torus centered on the planet, tilted to the planet's orbital plane.
- `InnerRadius` = 1.2× planet radius, `OuterRadius` = 3–6× planet radius, `Thickness` = 0.1–0.3× radius.
- Asteroid positions sampled uniformly within the torus volume.

**Supernova Clusters** — placed at random positions 15–45 km from zone center, avoiding overlap with planets:
- Spherical shell with density falling off ∝ 1/r² from center (denser core, sparser edges).
- Optional radial "streaks" — a fraction of asteroids placed along radial rays to simulate ejection patterns.
- `InnerRadius` ≈ 0 (or small), `OuterRadius` = 2–8 km.

**Asteroid Instance Generation** — for each cluster:
1. Sample position from cluster shape.
2. Pick mesh hash from a small set of asteroid mesh variants (e.g., `Asteroid01`–`Asteroid05`).
3. Random rotation axis (unit sphere), random rotation speed (0.05–0.5 rad/s).
4. Resource amount = uniform random in `[MinResource, MaxResource]`.
5. Bounding radius = uniform random in `[MinRadius, MaxRadius]`.
6. Color tinted by resource type.

### 3.2 MiningSystem (GameLogic)

**Files**: `MiningSystem.h`, `MiningSystem.cpp`

Integrates with `CombatSystem` collision processing. When a projectile (or a new "mining beam" weapon type) hits an asteroid, extract resources rather than dealing HP damage.

```
┌─────────────────────────────────────────────┐
│              MiningSystem                    │
│                                              │
│  Input:  CollisionEvents (projectile ↔ ast.) │
│  Output: Crate spawns, depletion events      │
│                                              │
│  1. Filter collisions: projectile vs asteroid │
│  2. Reduce asteroid ResourceAmount            │
│  3. Spawn Crate at asteroid position          │
│  4. If ResourceAmount ≤ 0 → depletion event   │
└─────────────────────────────────────────────┘
```

#### Key API

```cpp
class MiningSystem
{
public:
    MiningSystem(SpaceObjectManager& _manager);

    // Called after collision detection, before projectile despawn
    void ProcessCollisions(const std::vector<CollisionEvent>& _collisions);

    struct DepletionEvent
    {
        EntityHandle Asteroid;
        uint16_t     ClusterId;
        XMFLOAT3     Position;
    };

    [[nodiscard]] const std::vector<DepletionEvent>& GetDepletions() const noexcept;

private:
    SpaceObjectManager& m_manager;
    std::vector<DepletionEvent> m_depletions;
};
```

#### Mining Flow

1. Projectile hits asteroid → `MiningSystem::ProcessCollisions` checks if the projectile source has a mining-capable turret.
2. Subtract extraction amount (based on projectile damage scaled by a mining factor) from `AsteroidData::ResourceAmount`.
3. Spawn a `Crate` at the asteroid position with `LootTableId` derived from `ResourceType`.
4. When `ResourceAmount ≤ 0`: destroy the asteroid entity, emit a `DepletionEvent` so `RegrowthSystem` can queue regrowth.

### 3.3 RegrowthSystem (GameLogic)

**Files**: `RegrowthSystem.h`, `RegrowthSystem.cpp`

Tracks depleted asteroids per cluster and respawns them after a cooldown.

```
┌──────────────────────────────────────────────────┐
│               RegrowthSystem                      │
│                                                    │
│  Input:  DepletionEvents from MiningSystem         │
│  Output: New asteroid entities in the cluster      │
│                                                    │
│  Per tick:                                         │
│   1. Enqueue new depletions (timer = RespawnInt.)  │
│   2. Decrement all active timers                   │
│   3. For expired timers:                           │
│      a. Generate random position within cluster    │
│      b. Create asteroid entity                     │
│      c. Remove from queue                          │
└──────────────────────────────────────────────────┘
```

#### Key API

```cpp
class RegrowthSystem
{
public:
    RegrowthSystem(SpaceObjectManager& _manager, std::vector<AsteroidCluster>& _clusters);

    void EnqueueDepletion(const MiningSystem::DepletionEvent& _event);
    void Update(float _deltaTime);

private:
    XMFLOAT3 GenerateSpawnPosition(const AsteroidCluster& _cluster);

    SpaceObjectManager&            m_manager;
    std::vector<AsteroidCluster>&  m_clusters;
    std::mt19937                   m_rng;       // Seeded from universe seed
};
```

#### Regrowth Rules

- Each cluster has `RespawnInterval` (default 300 s = 5 min). Could vary by resource rarity.
- A cluster never exceeds its `AsteroidCount` cap — if `ActiveCount == AsteroidCount`, no regrowth queues are processed.
- Respawned asteroids get full `MaxResource`, a fresh random position within the cluster shape, and a new random rotation.
- The PRNG is seeded from the universe seed + cluster ID so regrowth positions are reproducible given the same depletion order (important for persistence).

---

## 4. Persistence

### 4.1 Strategy

Two-layer persistence:

| Layer | Content | Format | When |
|---|---|---|---|
| **Seed** | `uint64_t` universe seed | Server config / zone file | Written once at universe creation |
| **Delta** | Asteroid depletion state, regrowth timers, cluster active counts | Binary file (`universe_state.bin`) | Saved periodically + on shutdown |

On startup:
1. `UniverseGenerator::Generate(seed)` → produces the canonical layout.
2. Load `universe_state.bin` → apply delta: skip spawning depleted asteroids, restore regrowth timers.
3. Resume simulation.

### 4.2 Delta File Format

```
Header:
  uint32_t  Magic = 'UNIV'
  uint32_t  Version = 1
  uint64_t  Seed
  uint32_t  ServerTickAtSave
  uint32_t  ClusterCount

Per cluster:
  uint16_t  ClusterId
  uint32_t  ActiveCount
  uint32_t  RegrowthQueueSize
  Per regrowth entry:
    float   TimerRemaining

  uint32_t  DepletedAsteroidCount
  Per depleted asteroid:
    uint32_t  OriginalSpawnIndex  // Index into Generate() output for this cluster
```

The `OriginalSpawnIndex` lets the loader know which asteroids from the canonical generation should NOT be spawned because they are currently depleted.

### 4.3 Save Frequency

- **Periodic**: every 60 seconds (configurable via `ServerConfig`).
- **On shutdown**: `ServerLoop::Shutdown()` triggers a final save.
- **On depletion event**: optional — only if persistence latency matters.

---

## 5. Integration with Existing Systems

### 5.1 Zone Tick Pipeline — Updated

```
Zone::Tick(deltaTime)
  1.  m_commands.ProcessCommands()
  2.  m_npcAI.Update(deltaTime)
  3.  m_combat.Update(deltaTime)
  4.  m_movement.Update(deltaTime)
  5.  m_projectiles.Update(deltaTime)
  6.  m_collision.RebuildSpatialHash()
  7.  m_collision.DetectCollisions()
  8.  m_combat.ProcessCollisions(collisions)
  9.  m_mining.ProcessCollisions(collisions)          // NEW — after combat, before despawn
  10. m_projectiles.DespawnProjectiles(hits + miningHits)
  11. m_loot.ProcessCollisions(collisions)
  12. m_docking.ProcessCollisions(collisions)
  13. m_regrowth.EnqueueDepletions(m_mining)           // NEW
  14. m_regrowth.Update(deltaTime)                     // NEW
  15. // Destroyed ships → crate spawning + fleet cleanup
  16. m_jumpgates.Update()
  ++m_tickCount
```

Steps 9, 13, 14 are new. Mining runs after combat collision processing so that combat projectiles hitting asteroids don't double-process.

### 5.2 Zone Members — New

```cpp
// In Zone.h
GameLogic::MiningSystem     m_mining;
GameLogic::RegrowthSystem   m_regrowth;

// Universe data (owned by Zone, populated by UniverseGenerator)
std::vector<GameLogic::AsteroidCluster> m_clusters;
uint64_t m_universeSeed = 0;
```

### 5.3 ZoneLoader Changes

`ZoneLoader` gets a new method:

```cpp
static bool LoadUniverse(Zone& _zone, uint64_t _seed,
    const std::wstring& _deltaFilePath);
```

This replaces `CreateTestZone` for production use. It calls `UniverseGenerator::Generate(_seed)`, spawns all entities via `SpaceObjectManager::CreateEntity`, then applies the delta file.

### 5.4 SpaceObjectManager Additions

Add component arrays and accessors for `Planet` and `Sun`:

```cpp
std::vector<Neuron::PlanetData> m_planetData;
std::vector<Neuron::SunData>    m_sunData;

[[nodiscard]] PlanetData* GetPlanetData(EntityHandle _handle);
[[nodiscard]] SunData*    GetSunData(EntityHandle _handle);
```

### 5.5 StateBroadcaster

No protocol changes needed — planets, suns, and asteroids already map to `EntitySpawnMsg` (handle, category, mesh, position, orientation, color). The client renders them as static meshes via the existing `SpaceObjectCategory` dispatch.

### 5.6 ObjectDefs

Add `PlanetDef` and `SunDef` to `ObjectDefs.h` and `ObjectDefsLoader`. Minimal:

```cpp
struct PlanetDef : ObjectDefBase
{
    float MinOrbitRadius, MaxOrbitRadius;
    float MinRotationSpeed, MaxRotationSpeed;
};

struct SunDef : ObjectDefBase
{
    float MinLuminosity, MaxLuminosity;
};
```

---

## 6. Procedural Generation — Shape Algorithms

### 6.1 Ring Cluster (Torus)

Asteroid positions are sampled by:
1. Random angle θ ∈ [0, 2π) around the major axis (ring orbit).
2. Random angle φ ∈ [0, 2π) around the tube cross-section.
3. Tube radius r sampled uniformly in `[0, Thickness/2]`.
4. Major radius R sampled uniformly in `[InnerRadius, OuterRadius]`.
5. Point in local frame: `(R + r·cos(φ)) · cos(θ), r·sin(φ), (R + r·cos(φ)) · sin(θ)`.
6. Rotate by cluster `Orientation` quaternion, translate by `Center`.

This produces a classic torus distribution. For a flattened Saturn-style ring, set `Thickness` small relative to the radii.

### 6.2 Supernova Cluster (Debris Cloud)

Two sub-populations blended:

**Core cloud (70% of asteroids)**:
1. Random direction on unit sphere (uniform).
2. Random distance d from center, distributed ∝ d⁻² (inverse-square) clamped to `[InnerRadius, OuterRadius]`. Technique: `d = InnerRadius + (OuterRadius - InnerRadius) * (1 - sqrt(U))` where U ∈ [0, 1).
3. This concentrates asteroids toward the center — dense core, sparse halo.

**Radial streaks (30% of asteroids)**:
1. Pick N radial "rays" (3–8, chosen at generation time from the seed).
2. Each ray has a random direction vector and a cone half-angle (5°–15°).
3. Asteroids assigned to a ray are placed along the ray direction at random distances, with a small angular offset within the cone.
4. This simulates ejection plumes from an explosion.

The blend ratio and ray count are tunable per cluster.

---

## 7. Client-Side Rendering Considerations

No new client systems are required initially — the universe is spawned server-side and replicated to clients via existing `EntitySpawnMsg`. However:

- **New mesh assets** needed: planet meshes (spheres with color), sun meshes (emissive sphere), possibly new asteroid variants.
- **LOD / draw distance**: 3 000 asteroids at once may stress the renderer. Consider:
  - Server-side relevance filtering: only send asteroid spawns to clients within a visibility radius (e.g., 20 km draw distance).
  - Or rely on client-side frustum culling (already in the render pipeline) — 3 000 entities with simple meshes should be fine with instanced rendering.
- **Sun glow**: Future — could add a screen-space post-process for sun glow. Not required in first pass.
- **Render categories**: `SpaceObjectRenderer` already dispatches by category. Add cases for `Planet` and `Sun` in the render switch — they are just meshes with a color like everything else.

---

## 8. Implementation Plan

### Phase 1 — Foundation (types + generator)

| Step | Files | Description |
|---|---|---|
| 1.1 | `NeuronCore/GameTypes/SpaceObjectCategory.h` | Add `Planet`, `Sun` to enum |
| 1.2 | `NeuronCore/GameTypes/PlanetData.h` | New file — `PlanetData` struct |
| 1.3 | `NeuronCore/GameTypes/SunData.h` | New file — `SunData` struct |
| 1.4 | `NeuronCore/GameTypes/AsteroidData.h` | Add `ClusterId`, `MaxResource` fields |
| 1.5 | `NeuronCore/GameTypes/ObjectDefs.h` | Add `PlanetDef`, `SunDef` |
| 1.6 | `GameLogic/SpaceObjectManager.h/.cpp` | Add `m_planetData`, `m_sunData` vectors + accessors |
| 1.7 | `GameLogic/ObjectDefsLoader.h/.cpp` | Load `PlanetDef`, `SunDef` |
| 1.8 | Build + fix all category switch statements | Compiler will flag missing `Planet`/`Sun` cases |

### Phase 2 — Procedural Generation

| Step | Files | Description |
|---|---|---|
| 2.1 | `NeuronCore/GameTypes/AsteroidCluster.h` | New file — `ClusterShape` enum, `AsteroidCluster` struct |
| 2.2 | `GameLogic/UniverseGenerator.h/.cpp` | New files — seed-based generation of suns, planets, clusters, asteroids |
| 2.3 | `NeuronCore/NeuronRandom.h` | New file — thin PRNG wrapper around `std::mt19937` with helpers (float range, unit sphere, etc.) |
| 2.4 | `EarthRiseServer/ZoneLoader.h/.cpp` | Add `LoadUniverse()` method |
| 2.5 | Unit tests | Seed stability test: same seed → same layout. Boundary tests for torus/sphere sampling. |

### Phase 3 — Mining + Regrowth

| Step | Files | Description |
|---|---|---|
| 3.1 | `GameLogic/MiningSystem.h/.cpp` | New files — collision filtering, resource extraction, crate spawning |
| 3.2 | `GameLogic/RegrowthSystem.h/.cpp` | New files — depletion queue, timer tick, respawn |
| 3.3 | `EarthRiseServer/Zone.h/.cpp` | Add `m_mining`, `m_regrowth`, wire into tick pipeline |
| 3.4 | `GameLogic/CMakeLists.txt` | Add new source files |
| 3.5 | Unit tests | Mining depletion flow, regrowth timer, cluster cap enforcement |

### Phase 4 — Persistence

| Step | Files | Description |
|---|---|---|
| 4.1 | `GameLogic/UniversePersistence.h/.cpp` | New files — save/load delta state binary file |
| 4.2 | `EarthRiseServer/ServerLoop.cpp` | Periodic save timer, save-on-shutdown |
| 4.3 | `EarthRiseServer/ZoneLoader.cpp` | Apply delta on load in `LoadUniverse()` |
| 4.4 | `EarthRiseServer/ServerConfig.h` | Add `UniverseSeed`, `DeltaSavePath`, `DeltaSaveIntervalSec` |
| 4.5 | Integration test | Start server → mine asteroids → shutdown → restart → verify state |

### Phase 5 — Client Rendering + Polish

| Step | Files | Description |
|---|---|---|
| 5.1 | `GameRender/` | Add render cases for `Planet`, `Sun` categories |
| 5.2 | `GameData/Meshes/` | Add planet, sun, additional asteroid mesh variants |
| 5.3 | `Earthrise/ClientWorldState` | Handle new categories in spawn processing |
| 5.4 | Performance profiling | Validate 3 000 asteroids render within frame budget |

---

## 9. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| 3 000 entities exceed render budget | Frame drops on client | Server-side relevance filtering (only send nearby asteroids); instanced rendering on client |
| SpatialHash performance with 3 000 static asteroids | Rebuild cost per tick | Asteroids are static — only insert once, skip rebuild for non-moving entities. Only rebuild moving entities. |
| Seed drift between server versions | Universe layout changes after code update | Pin generation algorithm behind a version number; test seed stability in CI |
| Delta file corruption | Lost mining progress | Checksum in header; keep one backup copy (`universe_state.bak`) |
| Cluster overlap with planets/stations | Asteroids inside a planet | Exclusion zone check during generation — reject samples too close to any celestial body |

---

## 10. Future Extensions

- **Gravity wells** — planets apply radial force to nearby ships (gameplay impact).
- **Heat zones** — suns deal damage to ships within a radius.
- **Orbital mechanics** — planets slowly orbit suns; ring clusters orbit with their parent planet.
- **Resource rarity tiers** — different resource types with varying regrowth rates and value.
- **Asteroid destruction effects** — particle burst when asteroid is depleted.
- **Nebulae** — large volumetric decoration regions that affect sensor range.
- **Multi-zone expansion** — generate multiple star systems connected by jumpgates, each with its own seed.
- **Client-side procedural detail** — client uses the same seed to add visual-only detail (small rocks, dust) without server entities.
