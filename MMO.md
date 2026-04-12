# Earthrise — MMO Space Game: High-Level Implementation Plan

## Table of Contents

- [1. Current State Analysis](#1-current-state-analysis)
- [2. Open Questions](#2-open-questions)
- [3. Target Architecture](#3-target-architecture)
- [4. Visual Style — Eighties / Darwinia Aesthetic](#4-visual-style--eighties--darwinia-aesthetic)
- [5. Space Object Taxonomy](#5-space-object-taxonomy)
- [6. Phase Breakdown](#6-phase-breakdown)
  - [Phase 1 — Foundation & Shared Core](#phase-1--foundation--shared-core)
  - [Phase 2 — Entity System & Space Object Data Model](#phase-2--entity-system--space-object-data-model)
  - [Phase 3 — CMO Asset Pipeline & Flat-Color Rendering](#phase-3--cmo-asset-pipeline--flat-color-rendering)
  - [Phase 4 — Networking Layer](#phase-4--networking-layer)
  - [Phase 5 — Server Simulation Loop](#phase-5--server-simulation-loop)
  - [Phase 6 — Client Game Loop & World Sync](#phase-6--client-game-loop--world-sync)
  - [Phase 7 — Gameplay Systems](#phase-7--gameplay-systems)
  - [Phase 8 — HUD, UI & Player Interaction](#phase-8--hud-ui--player-interaction)
  - [Phase 9 — Audio & Effects](#phase-9--audio--effects)
  - [Phase 10 — Local Server Deployment & Database](#phase-10--local-server-deployment--database)
  - [Phase 11 — Polish, Optimization & Ship Readiness](#phase-11--polish-optimization--ship-readiness)
- [7. Testing Strategy](#7-testing-strategy)
- [8. Risk Register](#8-risk-register)
- [9. Conventions & Standards](#9-conventions--standards)

---

## 1. Current State Analysis

### Project Structure (CMake)

| Project | Type | Role | Dependencies |
|---|---|---|---|
| **NeuronCore** | Static lib | Engine foundation: math, timers, events, file I/O, async, debug, data reader/writer, tasks/threads | — |
| **NeuronServer** | Static lib | Server-side engine layer (stub — contains only `pch.h` → `NeuronServer.h` → `NeuronCore.h`) | NeuronCore |
| **NeuronClient** | Static lib | Client engine: DirectX 12 graphics core, descriptor heaps, audio engine, sound effects, PIX profiling | NeuronCore |
| **Earthrise** | Win32 desktop application | Client executable: `GameApp` (extends `GameMain`), main loop, windowing | NeuronClient, NeuronCore |
| **EarthRiseServer** | Console application | Server executable (empty `main()`) | NeuronServer, NeuronCore |

### What Already Exists (Boilerplate to Reuse)

| Subsystem | Status | Key Files |
|---|---|---|
| Math (DirectXMath wrapper) | ✅ Ready | `GameMath.h`, `MathCommon.h` — `Neuron::Math` namespace with SIMD helpers, vector constants |
| Timer | ✅ Ready | `TimerCore.h` — variable timestep, QPC-based, frame counting |
| Event System | ✅ Ready | `Event.h`, `EventManager.h/.cpp` — type-safe publish/subscribe, WndProc dispatch, `EventSubscriber` base class |
| File I/O | ✅ Ready | `FileSys.h/.cpp` — binary & text file reading, home directory management |
| Debug / Assert | ✅ Ready | `Debug.h` — `DebugTrace`, `Fatal`, `ASSERT`, `DEBUG_ASSERT` |
| Helpers | ✅ Ready | `NeuronHelper.h` — `ENUM_HELPER` macro, `NonCopyable`, `ScopedHandle`, enum utilities |
| Async Loader | ✅ Ready | `ASyncLoader.h` — atomic loading state tracking (instance & static variants) |
| Tasks / Threads | ✅ Ready | `TasksCore.h/.cpp` — `Neuron::Tasks::Thread` (start/stop/wait/bind), periodic timer helpers |
| Data Serialization | ⚠️ Partial | `DataReader.h`, `DataWriter.h` — depend on `NetLib.h` which **does not exist** yet; needs `DATALOAD_SIZE` constant |
| DirectX 12 Core | ✅ Ready | `GraphicsCore.h/.cpp` — device, swap chain, command list, fence, descriptor allocators |
| Descriptor Heaps | ✅ Ready | `DescriptorHeap.h/.cpp` — CPU-visible unbounded allocator, shader-visible heap handles |
| DirectX Helpers | ✅ Ready | `DirectXHelper.h` — `IID_GRAPHICS_PPV_ARGS`, `d3dx12.h`, PIX naming |
| Color Library | ✅ Ready | `Color.h` — full named-color constant table (`XMVECTORF32`) |
| Audio Engine | ✅ Ready | `AudioEngine.h/.cpp`, `SoundEffect`, `SoundEffectInstance`, `WAVFileReader` — XAudio2-based |
| PIX Profiler | ✅ Ready | `PixProfiler.h` — CPU & GPU `ProfileScope` RAII guards |
| GameMain Base Class | ✅ Ready | `GameMain.h` — abstract `Startup/Shutdown/Update/RenderScene/RenderCanvas` lifecycle |
| Client Engine | ✅ Ready | `NeuronClient.h/.cpp` — window creation, device init, main loop hookup |
| Client App Shell | ✅ Ready | `GameApp.h/.cpp`, `WinMain.cpp` — language detection, `ClientEngine::Startup`, message pump |
| CMO Mesh Assets | ✅ Ready | `Assets/Mesh/<type>/*.cmo` — 65 low-poly meshes across 9 category subfolders (see §5 for full inventory) |
| Server Shell | ⚠️ Stub | `EarthRiseServer/WinMain.cpp` — empty `main()` |

### Detected Versions

| Component | Version |
|---|---|
| Build System | CMake 3.28+ (Ninja or Visual Studio generator) with vcpkg toolchain |
| C++ Standard | C++23 (`CMAKE_CXX_STANDARD 23`) |
| CppWinRT | via vcpkg (`cppwinrt` package) |
| PIX Event Runtime | via vcpkg (optional feature `pix`) |
| CRT Linkage | Static (`/MT`, `/MTd`) |
| Platform | x64 only |
| Min Windows Version | 10.0.17763.0 |

### Missing Pieces (Must Be Created)

| Component | Notes |
|---|---|
| `NetLib.h` | Required by `DataReader.h`/`DataWriter.h`; must define `DATALOAD_SIZE` and network protocol primitives |
| `GameLogic` project | Mentioned in copilot instructions as a standalone project; does not exist in the CMake build yet |
| `GameRender` project | Mentioned in copilot instructions for object renderers; does not exist in the CMake build yet |
| Entity System | No entity/component/handle infrastructure |
| CMO Loader | 65 `.cmo` meshes exist under `Assets/Mesh/<type>/` but no loader code exists |
| Rendering Pipeline | No mesh rendering, no shaders beyond swap-chain present |
| Networking | Winsock2 headers included but no socket code |
| Server Loop | Empty `main()` |
| Spatial Partitioning | None |
| UI / HUD | None |
| `BinaryFile::WriteFile` + serialization helpers | `DataReader`/`DataWriter` are hardcoded to `DATALOAD_SIZE` (1400 bytes) — unusable for file I/O (zone definitions, object defs). Extend existing `FileSys`/`BinaryFile` with `WriteFile` and add a CRTP serialization base (`SerializationBase.h`) providing `Read<T>`/`Write<T>` over `byte_buffer_t` |
| NeuronCore header split | `NeuronCore.h` unconditionally includes WinRT projections (`winrt/Windows.Foundation.Collections.h`, etc.) and `TasksCore.h` uses `Windows::System::Threading::ThreadPoolTimer`. Server builds targeting Windows Server Core containers may not have these APIs available. Need a server-safe header tier |

---

## 2. Open Questions

### Confirmed Answers

| # | Question | Answer |
|---|---|---|
| 1 | CMO format | **DirectXTK `.cmo`** — colors are **assigned at runtime** per-object/per-material, not baked into the CMO |
| 2 | Visual style | **Darwinia-inspired** — see §4 below for detailed breakdown from reference image |
| 3 | Camera / perspective | **First-person view** with **multi-unit RTS command** — Homeworld-style: free camera in 3D, attachable to flagship, with fleet command overlay |
| 4 | World topology | **Single zone** — one contiguous play area |
| 5 | Authority model | **Fully server-authoritative** — server owns all simulation state |
| 6 | Packet size / protocol | **MTU-safe UDP** — `DATALOAD_SIZE = 1400` bytes |
| 7 | GameLogic project | **Server-side simulation code** — not shared with client. Client only needs lightweight data structs for deserialization + rendering |
| 8 | Database | **Azure SQL (MS SQL)** for player persistence |
| R1 | Unit control | **Multi-unit** — player commands a fleet of ships (select, group, issue orders) |
| R2 | Jumpgates | **In-zone fast travel** — warp to other areas of the zone quickly |
| R3 | Camera | **First-person view** — camera attached to flagship or free-flying; combined with fleet command UI |
| R4 | Movement | **Full 3D** — no 2D plane constraint; ships move freely in all axes |
| R5 | Zone size | **Recommended: 100 km cube** (100,000 × 100,000 × 100,000 game units, 1 unit = 1 meter) — see §3.1 |
| R6 | Concurrent players | **Start with 100** per server instance |
| R7 | Persistence | **Objects and locations** — player fleet composition, ship loadouts, positions, inventory |
| R8 | Authentication | **Custom login** (username/password or token-based, stored in local MSSQL) |
| R9 | CI/CD | **CMake builds** (Ninja or VS generator via presets) — no external pipeline yet; Phase 10 will add automated build/deploy |

### ⚠️ Camera Design Note

The original answer was **"top-down, RTS style"** but the refined answer is **"First Person View"**.
These are fundamentally different. The plan now targets a **Homeworld-style hybrid**:

- **First-person / free camera**: The camera can attach to any ship in the fleet (chase-cam / cockpit) or fly freely in 3D space
- **Fleet command overlay**: While in first-person, the player can select ships, issue move/attack/dock orders via UI overlay
- **Tactical zoom**: Scroll wheel zooms out to a broader fleet view for strategic command, zoom in for first-person combat
- **Full 3D**: Camera and movement operate in all three axes — no 2D plane restriction

---

## 3. Target Architecture

### Dependency Graph

Since the game uses a **fully server-authoritative** model, simulation logic lives on the server.
The client only needs lightweight data types (enums, struct definitions, serialization) to
deserialize state updates and render. A thin **GameTypes** header set in NeuronCore provides
the shared definitions; **GameLogic** contains the full simulation and links only to the server.

### 3.1 Zone Size Recommendation

| Parameter | Value | Rationale |
|---|---|---|
| **Zone dimensions** | 100,000 × 100,000 × 100,000 units (100 km cube) | Large enough for strategic depth with jumpgate fast-travel; small enough for server to simulate with 100 players |
| **Unit scale** | 1 unit = 1 meter | Ships are 20–200 m long; stations 500–2,000 m; asteroids 50–5,000 m |
| **Ship speed range** | 50–500 units/sec (180–1,800 km/h) | Full zone traversal: 200–2,000 sec; jumpgates cut this to near-instant |
| **Area of interest radius** | 10,000 units (10 km) | Full-detail entity updates sent to clients within this range of their camera |
| **Spatial hash cell size** | 2,000 units (2 km) | Spatial hash (key = `hash(x/cell, y/cell, z/cell)` → bucket) — only occupied cells consume memory; O(1) insert/query; efficient for sparse 3D space |
| **Draw distance** | 20,000 units (20 km) | Beyond this, entities are LOD-reduced or culled |
| **Jumpgate count** | 4–8 per zone | Distributed to create strategic chokepoints and fast-travel routes |

### 3.2 Dependency Graph

```
                           ┌─────────────────────────┐
                           │       NeuronCore         │
                           │ (math, events, file,     │
                           │  timer, tasks, debug,    │
                           │  NetLib, DataReader/     │
                           │  DataWriter,             │
                           │  GameTypes: entity       │
                           │  handle, enums, structs) │
                           └────────────┬─────────────┘
                                        │
                    ┌───────────────────┼───────────────────┐
                    │                   │                   │
           ┌────────▼───────┐  ┌────────▼────────┐  ┌──────▼────────┐
           │  NeuronClient  │  │  NeuronServer   │  │   GameLogic   │
           │ (DX12 graphics,│  │ (server engine  │  │ (simulation:  │
           │  audio, input, │  │  bootstrap,     │  │  movement,    │
           │  windowing)    │  │  socket accept) │  │  collision,   │
           └────────┬───────┘  └────────┬────────┘  │  combat, AI,  │
                    │                   │           │  spawning,    │
           ┌────────▼───────┐           │           │  zone logic)  │
           │   GameRender   │           │           └──────┬────────┘
           │ (CMO loader,   │           │                  │
           │  flat-color    │           │                  │
           │  pipeline,     │           │    ┌─────────────┘
           │  per-category  │           │    │
           │  renderers,    │           │    │
           │  camera, grid, │           │    │
           │  bloom)        │           │    │
           └────────┬───────┘           │    │
                    │                   │    │
           ┌────────▼───────┐  ┌────────▼────▼────┐
           │   Earthrise    │  │  EarthRiseServer  │
           │ (client app,   │  │ (server app,      │
           │  game loop,    │  │  zone management, │
           │  input, HUD,   │  │  sessions,        │
           │  interpolation,│  │  state broadcast, │
           │  UI)           │  │  persistence)     │
           └────────────────┘  └───────────────────┘

                    ┌────────────────┐
                    │ EarthRiseTests │
                    │ (unit + integ  │
                    │  tests)        │
                    └────────────────┘
```

### Key Architectural Decision: Simulation Code Placement

| Option | Pros | Cons | Decision |
|---|---|---|---|
| **GameLogic = server-only** (simulation systems link only to EarthRiseServer) | Clean separation; client never has simulation code; no risk of client-side cheating via shared code | Shared data structs needed by both sides must live elsewhere (NeuronCore) | ✅ **Chosen** |
| GameLogic = shared (client + server) | Client can run local prediction with identical physics | Larger client binary; shared code is a cheat surface; coupling risk | ❌ Rejected |

**Consequence**: Entity data types (`SpaceObjectCategory`, `EntityHandle`, `SpaceObject` struct, per-category structs) are placed in **NeuronCore** as a `GameTypes/` header group. Full simulation systems (movement, collision, combat, AI) live in **GameLogic**, which only the server links.

### Module Responsibilities

| Module | Owns | Does NOT Own |
|---|---|---|
| **NeuronCore** | Math, timers, events, file I/O (`FileSys`/`BinaryFile`/`TextFile`), async, tasks, threads, debug, serialization (`DataReader`/`DataWriter` for network, `BinaryFile` + `SerializationBase` for disk), `NetLib` (UDP socket, packet framing, `DATALOAD_SIZE = 1400`), **GameTypes** (entity handle, `SpaceObjectCategory` enum, `SpaceObject` struct, per-category data structs, object definition types). Headers split into `NeuronCoreBase.h` (server-safe, no WinRT) and `NeuronCoreWinRT.h` (client) | Simulation logic, rendering, audio |
| **GameLogic** | **Server-only** simulation: 3D movement system, collision system (3D spatial hash), combat system, fleet management, jumpgate warp, AI, spawning, zone simulation tick, command processing, data-driven object definitions loading (binary format via `BinaryFile`) | Rendering, audio, networking transport, UI, data type definitions (those are in NeuronCore/GameTypes) |
| **NeuronClient** | DX12 device lifecycle, descriptor management, audio engine, WAV loading, PIX profiling, window management | Game-specific rendering, entity logic |
| **NeuronServer** | Server engine bootstrap, UDP socket listener, session management, zone thread orchestration | Game rules, entity behavior |
| **GameRender** | CMO mesh loader, flat-color shader pipeline, per-category renderers (`ShipRenderer`, `AsteroidRenderer`, `StationRenderer`, etc.), multi-mode camera (first-person / free-fly / tactical zoom), tactical grid, starfield, bloom post-processing, particle system | Entity logic, networking, audio |
| **Earthrise** | Client `GameApp`, main loop, first-person flight + fleet command input, HUD/UI overlay (fleet panel, targeting, jumpgate UI), entity interpolation, server connection (UDP), world state mirror, custom login flow | Server simulation logic |
| **EarthRiseServer** | Server `main()`, AKS health probes, config loading, zone lifecycle, player session management, state broadcast, Azure SQL persistence | Client rendering |
| **EarthRiseTests** | Unit tests (GameLogic, NeuronCore, serialization, math) and integration tests (client-server protocol round-trip) | Production code |

---

## 4. Visual Style — Eighties / Darwinia Aesthetic

> **Confirmed** — based on the provided Darwinia reference screenshot.

### Reference Analysis (Darwinia Screenshot)

The Darwinia aesthetic is characterized by:

| Element | Description | Earthrise Adaptation |
|---|---|---|
| **Flat shading** | No textures; per-face solid colors with visible polygon facets | CMO meshes rendered with runtime-assigned flat diffuse colors, no smoothing |
| **Low-poly geometry** | Intentionally angular, coarse tessellation — geometry is the art style | All space objects are low-poly CMOs; facets are a feature, not a limitation |
| **Wireframe grid plane** | Blue-cyan wireframe grid as a ground/reference surface | **3D reference grid**: optional targeting/command plane rendered at the fleet's average Y-level; also used as a tactical overlay when zoomed out; fades with distance |
| **Neon color palette** | Bright saturated accents — green, cyan, magenta, orange, yellow — on very dark backgrounds | Ships, stations, projectiles use neon accents; space background is deep black/navy |
| **Bloom / glow** | Intense bloom on explosions, energy effects, and bright objects | Post-process bloom on weapon fire, engine trails, explosions, jumpgates |
| **Small geometric entities** | Diamond shapes, stick figures, small particles as units/pickups | Crates as small geometric shapes; decorations as scattered diamond/particle sprites |
| **Dark atmosphere** | Scene dominated by darkness, objects pop with color contrast | Deep-space black with subtle star particles; all gameplay elements are bright against void |
| **Visible edges** | Polygon edges slightly visible due to flat shading and angular geometry | No wireframe overlay needed — flat shading on low-poly meshes naturally shows edges |
| **Minimal UI chrome** | Labels are simple floating text (e.g., "biosphere" label in screenshot) | Floating text labels for stations, jumpgates; minimal HUD borders |
| **Warm + cool contrast** | Warm colors (orange, gold, yellow) for terrain/structures; cool colors (blue, cyan, green) for units/grid | Stations/asteroids in warm tones; ships/projectiles in cool neons; grid in cyan/blue |

### Rendering Pipeline Requirements

1. **Vertex shader**: Standard MVP transform; pass flat (per-face or per-triangle) color to pixel shader
2. **Pixel shader**: Output flat diffuse color — no lighting model beyond basic ambient + single directional light for silhouette depth
3. **Runtime color assignment**: Each `SpaceObjectCategory` (or individual object definition) specifies its diffuse color; shader reads from a per-draw constant buffer
4. **Post-process bloom**: Bright pixels (above threshold) get Gaussian blur and additive blend — critical for the Darwinia glow effect
5. **Tactical grid**: Optional wireframe reference plane rendered at a configurable Y-level; cyan/blue color with distance-based alpha fade; used during fleet command mode for spatial reference in 3D space
6. **Background**: Solid dark color (near-black) with optional sparse star particle layer
7. **Particle effects**: Small geometric shapes (diamonds, squares) for explosions, pickups, ambient effects — matching the low-poly aesthetic
8. **No textures**: The entire pipeline operates without texture sampling; all color is procedural or constant
9. **First-person integration**: When attached to a ship, render a minimal cockpit frame or HUD overlay (Darwinia-style minimal chrome); engine glow and weapon effects visible from this perspective

---

## 5. Space Object Taxonomy

| Category | Asset Folder | Mesh Count | Mesh Examples | Behavior | Simulated? | Rendered? | Per Zone |
|---|---|---|---|---|---|---|---|
| **Asteroid** | `Assets/Mesh/Asteroids/` | 6 | Asteroid01–06 | Static or slow-drift obstacle; possibly mineable/destructible | ✅ Server | ✅ | 100–10,000 |
| **Crate** | `Assets/Mesh/Crates/` | 3 | Crate01, Crate02, CrateAlien01 | Lootable floating container; small geometric shape (Darwinia diamond style) | ✅ Server | ✅ | 10–500 |
| **Decoration** | `Assets/Mesh/Decorations/` | 4 | BrokenAsteroid01, Satellite01, Satellite01Rust, StationWreck01 | Cosmetic-only: debris, satellites, ambient geometric particles | ✅ Server spawns | ✅ | 100–5,000 |
| **Ship (Hull)** | `Assets/Mesh/Hulls/` | 30 | HullAsteria, HullAurora, HullAvalanche, …, HullYamato | Player or NPC vessel; full 3D movement; weapons, AI | ✅ Server | ✅ | 10–1,000 |
| **Jumpgate** | `Assets/Mesh/Jumpgates/` | 1 | Jumpgate01 | In-zone fast-travel teleportation point | ✅ Server | ✅ | 1–10 |
| **Projectile** | `Assets/Mesh/Projectiles/` | 5 | AlienSeeker, MissileHeavy, MissileLight, RocketHeavy, RocketLight | Short-lived damage entity; travels from turret to target | ✅ Server | ✅ | 0–5,000 |
| **SpaceObject** | `Assets/Mesh/SpaceObjects/` | 8 | DebrisGenericBarrel01, DebrisGenericWreck01, ErgrekTerminal, … | Generic base / catch-all (debris, quest objects) | ✅ Server | ✅ | — |
| **Station** | `Assets/Mesh/Stations/` | 5 | Business01, Military01, Mining01, Outpost01, Science01 | Large static structure; docking, services (trade, repair, fitting) | ✅ Server | ✅ | 1–20 |
| **Turret** | `Assets/Mesh/Turrets/` | 3 | AlienTurret01, BallisticTurret01, EnergyTurret01 | Weapon emplacement on ships or stations; auto-aim or player-directed | ✅ Server | ✅ | 10–500 |

**Total: 65 CMO meshes** across 9 subfolders. Folder names map 1:1 to `SpaceObjectCategory` values (with `Hulls/` → `Ship`).

### Proposed Enum (NeuronCore/GameTypes)

```cpp
enum class SpaceObjectCategory : uint8_t
{
    SpaceObject,  // Generic base
    Asteroid,
    Crate,
    Decoration,
    Ship,
    Jumpgate,
    Projectile,
    Station,
    Turret,

    COUNT         // Sentinel for ENUM_HELPER
};
ENUM_HELPER(SpaceObjectCategory, SpaceObject, Turret)
```

---

## 6. Phase Breakdown

Each phase is designed to produce a **buildable, testable** increment. No phase should leave the project in a broken state.

---

### Phase 1 — Foundation & Shared Core ✅ COMPLETED

**Goal**: Create the missing foundational pieces so that all projects build, and establish the shared data type layer in NeuronCore.

**Status**: All deliverables implemented. Debug and Release x64 builds verified.

#### Deliverables

| # | Task | Project | Files | Status |
|---|---|---|---|---|
| 1.1 | Create `NetLib.h` — define `DATALOAD_SIZE = 1400` (MTU-safe UDP), packet header struct, message ID enum, UDP socket primitives | NeuronCore | `NetLib.h` | ✅ Done |
| 1.2 | Verify `DataReader` / `DataWriter` compile with new `NetLib.h` | NeuronCore | — | ✅ Done |
| 1.3 | **Split `ENUM_HELPER` & fix postfix `++`** — refactor `NeuronHelper.h`: `ENUM_HELPER` provides sequential operators (++, --, iterator, range, `SizeOf`); new `ENUM_FLAGS_HELPER` adds bitwise operators (`\|`, `&`, `^`, `~`, `!`). Sequential enums like `SpaceObjectCategory` use `ENUM_HELPER`; flag enums use `ENUM_FLAGS_HELPER`. **Bug fix**: the existing postfix `operator++(T&, int)` returns the *new* value instead of the pre-increment value — fix to return old value before incrementing. Also mark redundant type-specific methods in `DataWriter` (e.g., `WriteInt16`, `WriteFloat`) as `[[deprecated]]` in favor of the generic `Write<T>()` template | NeuronCore | `NeuronHelper.h`, `DataWriter.h` | ✅ Done |
| 1.4 | Create **GameLogic** static lib project (server-only simulation), add `add_subdirectory(GameLogic)` to root `CMakeLists.txt`, configure include paths (includes NeuronCore) | GameLogic | `CMakeLists.txt`, `pch.h`, `pch.cpp`, `GameLogic.h` | ✅ Done |
| 1.5 | Create **GameRender** static lib project, add `add_subdirectory(GameRender)` to root `CMakeLists.txt`, configure include paths (includes NeuronClient, NeuronCore) | GameRender | `CMakeLists.txt`, `pch.h`, `pch.cpp`, `GameRender.h` | ✅ Done |
| 1.6 | Create **EarthRiseTests** native unit test project, add `add_subdirectory(EarthRiseTests)` to root `CMakeLists.txt` | EarthRiseTests | `CMakeLists.txt`, `pch.h`, `pch.cpp` | ✅ Done |
| 1.7 | Update `EarthRiseServer/CMakeLists.txt` to link GameLogic (server-only) | — | `CMakeLists.txt` edit | ✅ Done |
| 1.8 | Update `Earthrise/CMakeLists.txt` to link GameRender (client-only) | — | `CMakeLists.txt` edit | ✅ Done |
| 1.9 | **Fix `RenderCanvas()` call** — add `main->RenderCanvas()` between `RenderScene()` and `Graphics::Core::Present()` in `Earthrise/WinMain.cpp`. Currently `RenderCanvas()` is defined in `GameMain.h` but never called; Phase 8 depends on it for HUD/UI rendering. **Note**: `RenderCanvas()` renders into the *same* command list and render target as `RenderScene()` — there is no intermediate clear. It composites UI on top of the 3D scene. If a separate LDR render target is needed for tone-mapping, that infrastructure must be added in Phase 3D alongside bloom | Earthrise | `WinMain.cpp` | ✅ Done |
| 1.10 | Create `NeuronCore/GameTypes/` header group — `EntityHandle.h`, `SpaceObjectCategory.h` stubs (populated in Phase 2) | NeuronCore | `GameTypes/` headers | ✅ Done |
| 1.11 | Verify full CMake build in Debug and Release x64 | — | — | ✅ Done |
| 1.12 | **Extend `FileSys`/`BinaryFile`** — add `BinaryFile::WriteFile(const std::wstring&, const byte_buffer_t&)` for writing binary data to disk. Create `SerializationBase.h` — a CRTP base class template providing `Read<T>`/`Write<T>` over `byte_buffer_t` (`std::vector<uint8_t>`), shared by both `DataReader`/`DataWriter` (network, fixed 1400-byte) and new `BinaryDataReader`/`BinaryDataWriter` helpers (disk, unlimited). `BinaryFile::ReadFile` already returns `byte_buffer_t`; the new helpers wrap a `byte_buffer_t` with a cursor for structured deserialization. Not constrained by `DATALOAD_SIZE`. `DataReader`/`DataWriter` remain for network packets only | NeuronCore | `FileSys.h` (extended), `FileSys.cpp` (extended), `SerializationBase.h` | ✅ Done |
| 1.13 | **Split NeuronCore headers for server compatibility** — `NeuronCore.h` currently includes WinRT projections (`winrt/Windows.Foundation.Collections.h`, `winrt/Windows.Globalization.h`, etc.) and `TasksCore.h` uses `Windows::System::Threading::ThreadPoolTimer`. Create `NeuronCoreBase.h` (pure C++ STL + Win32 + Winsock — server-safe) and `NeuronCoreWinRT.h` (adds WinRT projections — client-only). `NeuronServer/pch.h` includes `NeuronCoreBase.h`; `NeuronClient/pch.h` includes full `NeuronCore.h`. Add a `CreateTimerQueueTimer`-based fallback in `TasksCore` for server builds | NeuronCore, NeuronServer | `NeuronCoreBase.h`, `NeuronCoreWinRT.h`, `TasksCore.h/.cpp` edits | ✅ Done — `NeuronCoreBase.h` created; `NeuronCore.h` refactored to build on top of it; `TimerCore.h` fixed (`throw_hresult` → `Neuron::Fatal`); `NeuronServer.h` updated to include `NeuronCoreBase.h` |
| 1.14 | **Verify swap chain presentation mode** — confirm `Graphics::Core::Present()` uses `DXGI_SWAP_EFFECT_FLIP_DISCARD` with configurable VSync (pass `syncInterval = 0` for uncapped, `1` for VSync). Document the current behavior. The main loop spin (`PeekMessage` without `Sleep`) relies on VSync for frame pacing; add a config option for uncapped mode | Earthrise | `WinMain.cpp`, `GraphicsCore.cpp` audit | ✅ Verified — `FLIP_DISCARD` with tearing support (syncInterval=0 with `DXGI_PRESENT_ALLOW_TEARING` when supported, syncInterval=1 otherwise) |
| 1.15 | **Verify full CMake build in Debug and Release x64** (re-verify after 1.12–1.14 changes) | — | — | ✅ Done — Both Debug and Release x64 builds pass |

#### Tests (Phase 1)

- Unit: `DataReader`/`DataWriter` round-trip test (write values, read them back, verify equality)
- Unit: `BinaryFile` + `BinaryDataReader`/`BinaryDataWriter` round-trip test (write arbitrary-length data via `BinaryFile::WriteFile`, read back via `BinaryFile::ReadFile`, deserialize with `BinaryDataReader`, verify — no 1400-byte limit)
- Unit: `EventManager` publish/subscribe/unsubscribe test
- Unit: `Neuron::Math` vector operations sanity checks
- Unit: Verify postfix `operator++` on `ENUM_HELPER` enums returns pre-increment value

---

### Phase 2 — Entity System & Space Object Data Model ✅ COMPLETED

**Goal**: Build the entity data types (shared via NeuronCore/GameTypes) and the server-side entity management (GameLogic).

> **Status**: All deliverables implemented. EntityHandle (32-bit compact, 20-bit index / 12-bit generation), SpaceObject flat struct, 8 per-category component structs (SoA layout), ObjectDefs type hierarchy with server-side loader, EntityPool (slot-map), SpaceObjectManager (owns all component arrays). 22 unit tests covering handle creation/reuse, pool alloc/free/exhaustion, manager create/destroy across all categories, iteration, serialization round-trip via DataWriter/DataReader, and BinaryDataWriter/BinaryDataReader. Both Debug and Release x64 builds pass.

#### Deliverables

| # | Task | Project | Files | Status |
|---|---|---|---|---|
| 2.1 | **Entity Handle** — typed `(index, generation)` handle, 32-bit compact representation | NeuronCore | `GameTypes/EntityHandle.h` | ✅ Done (Phase 1 stub, fully functional) |
| 2.2 | **SpaceObjectCategory** enum with `ENUM_HELPER` | NeuronCore | `GameTypes/SpaceObjectCategory.h` | ✅ Done (Phase 1 stub, fully functional) |
| 2.3 | **SpaceObject** flat struct (composition, not inheritance) — handle, category, transform (`XMFLOAT3` position, `XMFLOAT4` orientation), velocity, bounding radius, runtime color, mesh name hash, active flag. This is the **common component** shared by all entity categories. No virtual methods, no base-class pointer chasing — optimized for cache-friendly iteration by movement/rendering systems | NeuronCore | `GameTypes/SpaceObject.h` | ✅ Done |
| 2.4 | **Per-category component structs** (parallel SoA arrays, not inheritance) — `AsteroidData`, `CrateData`, `DecorationData`, `ShipData`, `JumpgateData`, `ProjectileData`, `StationData`, `TurretData`. Each contains only the category-specific fields (e.g., `ShipData` has shield/armor/hull HP, energy, turret slots; `ProjectileData` has lifetime, damage, owner handle). Stored in separate arrays keyed by `EntityHandle`, enabling per-system iteration without touching unrelated data (e.g., combat iterates `ShipData` only, movement iterates `SpaceObject` transforms only) | NeuronCore | `GameTypes/AsteroidData.h`, `GameTypes/ShipData.h`, etc. | ✅ Done — All 8 structs |
| 2.5 | **Object Definition data types** — data-driven templates (ship stats, asteroid variants, turret fire rates); each definition includes a **mesh filename** (e.g., `"HullAvalanche"`) that maps to `Assets/Mesh/<CategoryFolder>/<MeshName>.cmo` at load time; type definitions shared, loading logic server-only (uses `BinaryFile::ReadFile` + `BinaryDataReader` for disk I/O). Client uses mesh filename from spawn/state messages to look up the mesh in `MeshCache` | NeuronCore (types) + GameLogic (loading) | `GameTypes/ObjectDefs.h`, `GameLogic/ObjectDefsLoader.cpp` | ✅ Done |
| 2.6 | **Entity Pool** — slot-map backed fixed-capacity pool; alloc / free / lookup by handle (server-only) | GameLogic | `EntityPool.h`, `EntityPool.cpp` | ✅ Done |
| 2.7 | **SpaceObjectManager** — creates/destroys entities by category; owns the `SpaceObject` array (common component) and per-category `*Data` arrays (SoA layout); iterates active objects per-category for system dispatch. Provides `GetSpaceObject(EntityHandle)`, `GetShipData(EntityHandle)`, etc. (server-only) | GameLogic | `SpaceObjectManager.h`, `SpaceObjectManager.cpp` | ✅ Done |

> **Design Decision — Composition over Inheritance**: Per-category data uses **SoA component arrays** rather than a polymorphic `SpaceObject` hierarchy. Rationale: (1) cache-friendly iteration per-system (movement touches only transforms; combat touches only `ShipData`); (2) no virtual dispatch overhead in hot loops simulating 10,000+ entities at 20 Hz; (3) `SpaceObject` remains a POD-like flat struct suitable for trivial serialization. Each `*Data` struct is stored in a parallel array indexed by the same `EntityHandle` slot, so lookup is O(1).

#### Tests (Phase 2)

- Unit: EntityHandle creation, generation increment on reuse, invalid-handle detection
- Unit: EntityPool alloc/free/lookup, pool exhaustion behavior
- Unit: SpaceObjectManager create/destroy across all categories
- Unit: SpaceObject serialization round-trip via DataWriter/DataReader (verifies NeuronCore types + serialization)

---

### Phase 3 — CMO Asset Pipeline & Flat-Color Rendering ✅ COMPLETED

**Goal**: Load DirectXTK `.cmo` mesh files and render them with runtime-assigned flat colors. Establish the Darwinia visual style with first-person / tactical camera and 3D space rendering.

> **Status**: All deliverables implemented across four sub-phases. DX12 GPU infrastructure (upload heap, CB allocator, shader-visible heap, GPU resource manager, pipeline helpers). CMO binary parser handles all 65 meshes. Flat-color rendering pipeline with reverse-Z depth, origin-rebased camera system (FreeFly/ChaseCamera/TacticalZoom), per-category renderers, procedural starfield, bloom post-processing, tactical grid, SpriteBatch for UI. GameApp integrated with test scene (one mesh per category with Darwinia-style neon colors). Shaders compiled at runtime via D3DCompile with embedded HLSL. 12 unit tests covering reverse-Z math, origin rebasing, CmoLoader parsing (individual + batch validation of all 65 .cmo files), LoadFromMemory parity, vertex normal validation. Both Debug and Release x64 builds pass.

> **Note**: `GameApp::Startup()` returns `Windows::Foundation::IAsyncAction` (WinRT coroutine). Use `co_await` for async asset loading (CMO files, shader compilation) and leverage the `ASyncLoader` base class for loading-state tracking.

Phase 3 is split into four sub-phases. Each produces a buildable, testable increment.

#### Phase 3A — DX12 GPU Infrastructure ✅

These subsystems are prerequisites for all rendering and do not exist in the codebase.

| # | Task | Project | Files | Status |
|---|---|---|---|---|
| 3A.1 | **Upload heap** — ring-buffer backed GPU upload heap for per-frame dynamic data (constant buffers, vertex updates); double- or triple-buffered per swap chain frame | GameRender | `UploadHeap.h`, `UploadHeap.cpp` | ✅ Done |
| 3A.2 | **Constant buffer manager** — allocate per-draw constant buffer blocks from the upload heap; return GPU virtual address for root descriptor binding | GameRender | `ConstantBufferAllocator.h` (header-only) | ✅ Done |
| 3A.3 | **Root signature & PSO helpers** — utility functions to build root signatures (from descriptor tables / root constants / root CBV) and create graphics PSOs from shader bytecode; cache compiled PSOs by hash | GameRender | `PipelineHelpers.h`, `PipelineHelpers.cpp` | ✅ Done |
| 3A.4 | **Reverse-Z depth buffer** — configure projection matrices with reversed near/far (near = draw distance, far = near clip); clear depth to `0.0f`; use `D3D12_COMPARISON_FUNC_GREATER_EQUAL`. Prevents z-fighting at the 20 km draw distance | GameRender | `Camera.h` (projection setup), `FlatColorPipeline` (PSO depth comparison), `GraphicsCore.cpp` (clear 0.0f) | ✅ Done |
| 3A.5 | **Shader-visible descriptor heap ring allocator** — the existing `DescriptorAllocator` in `GraphicsCore` only handles CPU-visible heaps. Phase 3D bloom post-processing requires SRVs for intermediate render targets bound in shader-visible heaps. Create a per-frame ring allocator for the `D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV` shader-visible heap; bind via `SetDescriptorHeaps` once per frame | GameRender | `ShaderVisibleHeap.h`, `ShaderVisibleHeap.cpp` | ✅ Done |
| 3A.6 | **GPU resource manager** — after initial upload of vertex/index buffers via the upload heap, copy data to `D3D12_HEAP_TYPE_DEFAULT` resources for GPU-optimal access; release the upload staging memory. Provides `CreateStaticBuffer(data, size)` → default-heap `ID3D12Resource` | GameRender | `GpuResourceManager.h`, `GpuResourceManager.cpp` | ✅ Done |

**Tests (Phase 3A)**:
- Unit: ConstantBufferAllocator — verify 256-byte alignment math (100→256, 256→256, 257→512) ✅
- Unit: Reverse-Z projection — verify _33 ≈ 0.0, near maps to depth ~1.0, far maps to depth ~0.0 ✅

#### Phase 3B — Asset Pipeline ✅

> **Asset Convention**: All 65 game meshes live under `GameData/Meshes/<CategoryFolder>/<MeshName>.cmo`. The category folder maps to `SpaceObjectCategory` (with `Hulls/` → `Ship`). Mesh filenames (without extension) are the same strings stored in object definitions and sent in spawn messages.

| # | Task | Project | Files | Status |
|---|---|---|---|---|
| 3B.1 | **CMO Loader** — parse DirectXTK `.cmo` binary format, extract vertex positions, normals, indices; **ignore texture references and baked material colors** (colors assigned at runtime). Must handle all 65 existing meshes (various poly counts and submesh configurations across the 9 category folders) | GameRender | `CmoLoader.h`, `CmoLoader.cpp` | ✅ Done |
| 3B.2 | **Mesh** — GPU resource wrapper: vertex buffer, index buffer, submesh table (material index, index range); uses GpuResourceManager for GPU resource creation | GameRender | `Mesh.h`, `Mesh.cpp` | ✅ Done |
| 3B.3 | **MeshCache** — load-once asset cache keyed by relative path (e.g., `"Hulls/HullAvalanche"`); resolves to `GameData/Meshes/<key>.cmo`; returns mesh pointer. FNV-1a hash key. Provides `PreloadCategory(SpaceObjectCategory)` to batch-load all meshes in a category folder at startup | GameRender | `MeshCache.h`, `MeshCache.cpp` | ✅ Done |

**Folder → Category Mapping** (used by `MeshCache::PreloadCategory`):

| `SpaceObjectCategory` | Asset Subfolder |
|---|---|
| `Asteroid` | `GameData/Meshes/Asteroids/` |
| `Crate` | `GameData/Meshes/Crates/` |
| `Decoration` | `GameData/Meshes/Decorations/` |
| `Ship` | `GameData/Meshes/Hulls/` |
| `Jumpgate` | `GameData/Meshes/Jumpgates/` |
| `Projectile` | `GameData/Meshes/Projectiles/` |
| `SpaceObject` | `GameData/Meshes/SpaceObjects/` |
| `Station` | `GameData/Meshes/Stations/` |
| `Turret` | `GameData/Meshes/Turrets/` |

**Tests (Phase 3B)**:
- Unit: CMO loader parses `Asteroids/Asteroid01.cmo` — vertex/index counts are non-zero, submesh count ≥ 1, indices divisible by 3 ✅
- Unit: CMO loader parses `Hulls/HullShuttle.cmo` — all indices within vertex range ✅
- Unit: CMO loader handles all 65+ `.cmo` files without error (batch validation across 9 folders) ✅
- Unit: Vertex normals are unit length (validated on Mining01.cmo) ✅
- Unit: `LoadFromMemory` produces identical results to `LoadFromFile` ✅

#### Phase 3C — Flat-Color Rendering & Camera ✅

| # | Task | Project | Files | Status |
|---|---|---|---|---|
| 3C.1 | **Flat-color shaders** — vertex shader (MVP transform, `nointerpolation` for flat shading), pixel shader (flat diffuse color from per-draw constant buffer + basic ambient/directional light for depth), root signature (2 root CBVs: b0=frame, b1=draw), PSO with reverse-Z GREATER_EQUAL depth. Shaders compiled at runtime via D3DCompile with embedded HLSL | GameRender | `Shaders/FlatColorVS.hlsl`, `Shaders/FlatColorPS.hlsl`, `FlatColorPipeline.h/.cpp` | ✅ Done |
| 3C.2 | **SpaceObjectRenderer base** — bind mesh, set world transform via constant buffer allocator, set **runtime-assigned color**, draw all submeshes. Transposes matrices for HLSL column-major. BeginFrame sets per-frame CB (ViewProjection, CameraPosition, LightDirection, AmbientIntensity) | GameRender | `SpaceObjectRenderer.h`, `SpaceObjectRenderer.cpp` | ✅ Done |
| 3C.3 | **Per-category renderers** — base `CategoryRenderer` wrapping SpaceObjectRenderer + MeshCache; 8 thin derived types: `AsteroidRenderer`, `ShipRenderer`, `StationRenderer`, `JumpgateRenderer`, `CrateRenderer`, `DecorationRenderer`, `ProjectileRenderer`, `TurretRenderer` | GameRender | `CategoryRenderers.h` | ✅ Done |
| 3C.4 | **Camera system** — multi-mode: **(a)** First-person chase-cam with smooth lerp, **(b)** free-fly camera (yaw/pitch), **(c)** tactical zoom; **reverse-Z** perspective projection via swapped nearZ/farZ in `XMMatrixPerspectiveFovLH`; `BoundingFrustum` frustum culling. **Origin-rebasing**: camera always at origin in view space | GameRender | `Camera.h`, `Camera.cpp` | ✅ Done |
| 3C.5 | **Starfield** — 2000 stars on Fibonacci sphere distribution, point list rendering, separate root sig/PSO (no depth write), HashToFloat for brightness variation, 19 km sphere radius | GameRender | `Starfield.h`, `Starfield.cpp` | ✅ Done |
| 3C.6 | **Render Scene integration** — `GameApp::Startup()` initializes upload heap (4 MB/frame), SRV heap, camera (PI/4 FOV, 0.5–20000 range), all renderers, preloads test meshes. `GameApp::RenderScene()` clears RT (near-black) + depth (0.0f reverse-Z), renders starfield, then 8 test objects (Asteroid01, Crate01, HullShuttle, Jumpgate01, MissileLight, DebrisGenericBarrel01, Mining01, BallisticTurret01) with distinct neon colors and origin-rebased transforms | Earthrise | `GameApp.h`, `GameApp.cpp` | ✅ Done |

**Tests (Phase 3C)**:
- Unit: Origin rebasing — entity - camera = small rebased position ✅
- Unit: View matrix with origin rebasing — camera at origin transforms to (0,0,0) ✅
- Unit: FlatColorVertex size is 24 bytes ✅
- Unit: Fibonacci sphere distribution is uniform between hemispheres ✅

#### Phase 3D — Post-Processing & Tactical Overlays ✅

| # | Task | Project | Files | Status |
|---|---|---|---|---|
| 3D.1 | **Post-processing: Bloom** — 3-pass: bright-extract (threshold luminance), Gaussian blur (5-tap horizontal+vertical, half-res ping-pong), additive composite. Fullscreen triangle VS (no VB, SV_VertexID). Uses ShaderVisibleHeap for SRV bindings. Embedded HLSL compiled at runtime | GameRender | `PostProcess.h`, `PostProcess.cpp` | ✅ Done |
| 3D.2 | **Tactical Grid** — wireframe grid at configurable Y-level, 200 m spacing, 2000 m extent, cyan/blue coloring, distance fade (500 m → 2000 m), alpha blending, LINE topology, no depth write, toggle visibility | GameRender | `TacticalGrid.h`, `TacticalGrid.cpp` | ✅ Done |
| 3D.3 | **SpriteBatch** — minimal DX12 sprite batcher for solid-color quads. `Begin()`/`DrawRect()`/`End()` API with per-batch flush. Orthographic screen-space projection (origin top-left). Alpha blending, no depth test. Two-triangle quads via vertex batching (max 512 sprites/batch). Uses ConstantBufferAllocator for dynamic VB | GameRender | `SpriteBatch.h`, `SpriteBatch.cpp` | ✅ Done |

**Tests (Phase 3D)**:
- Integration: Bloom post-process on a bright object; visually verify glow effect
- Integration: Tactical grid renders at correct Y-level, fades with distance, toggles on/off

---

### Phase 4 — Networking Layer ✅

**Goal**: Implement the client↔server communication protocol over **UDP** (MTU-safe, 1400-byte datagrams). Reliable delivery layer built on top for login/chat/inventory; unreliable for state updates.

#### Deliverables

| # | Task | Project | Files | Status |
|---|---|---|---|---|
| 4.1 | **UDP Socket wrapper** — Winsock2 UDP socket, non-blocking send/recv, address management. `InitializeNetworking()`/`ShutdownNetworking()` for Winsock lifecycle. Move semantics, `MakeAddress()` helper | NeuronCore | `UdpSocket.h/.cpp` | ✅ Done |
| 4.2 | **Reliability layer** — sequence numbers, ACK tracking via `unordered_map`, resend queue with configurable timeout (250 ms default, 10 max resends), duplicate detection via `unordered_set` with bounded pruning, `BuildAck()` for ACK datagram construction | NeuronCore | `ReliableChannel.h/.cpp` | ✅ Done |
| 4.3 | **Packet framing** — `FramePacket()` and `ParsePacket()` inline functions. Message ID + sequence + flags + payload within 1400-byte datagrams. `MAX_PAYLOAD_SIZE` constant. Validates buffer bounds on both frame and parse | NeuronCore | `Packet.h` | ✅ Done |
| 4.4 | **Message definitions** — `LoginRequest`, `LoginResponse`, `DisconnectMsg`, `HeartbeatMsg`, `EntitySpawnMsg`, `EntityDespawnMsg`, `StateSnapshotMsg`, `PlayerCommandMsg`, `ChatMsg`. Each with `Write(DataWriter&)` / `Read(DataReader&)`. Supporting enums: `LoginResult`, `DisconnectReason`, `CommandType`, `ChatChannel` | NeuronCore | `Messages.h` | ✅ Done |
| 4.5 | **Server listener** — `SessionManager`: owns server UDP socket, polls datagrams, dispatches to `ClientSession` by address (packed uint64 key). Creates sessions on `LoginRequest`, sends `LoginResponse`, handles `Disconnect`/`Heartbeat`/`Ack`. Session timeout (30 s), max 100 clients. `ClientSession`: per-client state with `ReliableChannel`, `SendPacket()` and `SendReliable()` | NeuronServer | `SessionManager.h/.cpp`, `ClientSession.h/.cpp` | ✅ Done |
| 4.6 | **Client connector** — `ServerConnection`: background `std::thread` for recv loop + reliable resend. `Connect()` sends `LoginRequest` via reliable channel, thread processes `LoginResponse` to transition to `Connected`. `Disconnect()` sends graceful disconnect. `DrainMessages()` for main-thread consumption via `concurrent_queue`. Mutex-protected send path | Earthrise | `ServerConnection.h/.cpp` | ✅ Done |
| 4.7 | **Bandwidth throttling** — `BandwidthManager`: per-client byte budget (default 64 KB/s) with 1-second sliding window reset. Area-of-interest filtering via `IsInAreaOfInterest()` (SIMD distance check, default 10 km radius). Snapshot rate limiting (20 Hz min interval). `RemoveClient()` cleanup | NeuronServer | `BandwidthManager.h/.cpp` | ✅ Done |

**Tests (Phase 4)**:
- Unit: Packet frame → parse round-trip with payload ✅
- Unit: Frame empty payload (header only) ✅
- Unit: Parse rejects truncated datagram ✅
- Unit: Parse rejects oversized payload claim ✅
- Unit: Frame rejects oversized payload ✅
- Unit: PacketHeader is 8 bytes ✅
- Unit: LoginRequest serialize/deserialize round-trip ✅
- Unit: LoginResponse serialize/deserialize round-trip ✅
- Unit: DisconnectMsg serialize/deserialize round-trip ✅
- Unit: HeartbeatMsg serialize/deserialize round-trip ✅
- Unit: EntitySpawnMsg serialize/deserialize round-trip ✅
- Unit: EntityDespawnMsg serialize/deserialize round-trip ✅
- Unit: StateSnapshotMsg serialize/deserialize (2 entities) round-trip ✅
- Unit: PlayerCommandMsg serialize/deserialize round-trip ✅
- Unit: ChatMsg serialize/deserialize round-trip ✅
- Unit: ReliableChannel queue, resend after timeout, ACK removes from pending ✅
- Unit: ReliableChannel duplicate detection ✅
- Unit: ReliableChannel sequence increments per send ✅
- Unit: ReliableChannel no resend before timeout ✅
- Unit: ReliableChannel BuildAck frames correctly ✅
- Unit: BandwidthManager budget enforced per second ✅
- Unit: BandwidthManager area-of-interest filtering ✅
- Unit: BandwidthManager snapshot rate limiting ✅
- Unit: BandwidthManager RemoveClient clears budget ✅
- Unit: Full write→frame→parse→read round-trip (ChatMessage) ✅
- Unit: Full write→frame→parse→read round-trip (EntitySpawn) ✅

---

### Phase 5 — Server Simulation Loop ✅ COMPLETED

**Goal**: The server runs a **fixed-timestep** game simulation in a **single 100 km³ zone**, processes fleet commands, and broadcasts world state snapshots over UDP to up to **100 concurrent players**.

#### Deliverables

| # | Task | Project | Files | Status |
|---|---|---|---|---|
| 5.1 | **Server main loop** — fixed-timestep tick (20 Hz), process input queue, simulate, broadcast state | EarthRiseServer | `WinMain.cpp` expansion, `ServerLoop.h/.cpp` | ✅ Done |
| 5.2 | **Zone** — the single persistent 100 km³ simulation region; owns a `SpaceObjectManager`, player list, 3D spatial hash | EarthRiseServer | `Zone.h`, `Zone.cpp` | ✅ Done |
| 5.3 | **Zone loader** — load zone definition from data file; `CreateTestZone` populates asteroids, station, jumpgate pair | EarthRiseServer | `ZoneLoader.h/.cpp` | ✅ Done |
| 5.4 | **Input processing** — dequeue fleet commands (move-to-position-3D, attack-target, dock, loot, use-ability, warp-to-jumpgate), apply to player's fleet entities | GameLogic | `InputCommand.h`, `CommandProcessor.h/.cpp` | ✅ Done |
| 5.5 | **Movement system** — full 3D steering/arrival behavior, velocity clamping, rotation toward target (quaternion-based); homing projectile steering | GameLogic | `MovementSystem.h/.cpp` | ✅ Done |
| 5.6 | **Collision detection** — broad-phase 3D spatial hash (2 km logical cells, hash-mapped — only occupied cells allocate memory), narrow-phase sphere-sphere tests, generate collision events | GameLogic | `CollisionSystem.h/.cpp`, `SpatialHash.h/.cpp` | ✅ Done |
| 5.7 | **State broadcast** — serialize entities within 10 km area-of-interest radius of each player's camera, send snapshots via UDP (1400-byte datagrams) | EarthRiseServer | `StateBroadcaster.h/.cpp` | ✅ Done |
| 5.8 | **Jumpgate fast-travel** — server-side: player issues warp command → ship teleported to destination jumpgate position; proximity check + fleet teleportation with spacing | GameLogic | `JumpgateSystem.h/.cpp` | ✅ Done |

**Tests (Phase 5)**:
- Unit: SpatialHash — insert entity, query at position → found ✅
- Unit: SpatialHash — remove entity → cell empty ✅
- Unit: SpatialHash — distant entity excluded from query ✅
- Unit: SpatialHash — clear removes all entities ✅
- Unit: SpatialHash — same-cell entities all returned ✅
- Unit: SpatialHash — update moves entity to new cell ✅
- Unit: MovementSystem — velocity integration updates position ✅
- Unit: MovementSystem — move target causes movement toward target ✅
- Unit: MovementSystem — ship arrives at target and stops ✅
- Unit: MovementSystem — clear move target stops targeting ✅
- Unit: CollisionSystem — overlapping spheres → collision detected ✅
- Unit: CollisionSystem — distant entities → no collision ✅
- Unit: CollisionSystem — decorations are skipped ✅
- Unit: CollisionSystem — no duplicate collision pairs ✅
- Unit: CommandProcessor — MoveTo command sets move target on fleet ✅
- Unit: CommandProcessor — unregistered player command ignored ✅
- Unit: CommandProcessor — fleet registration and unregistration ✅
- Unit: JumpgateSystem — fleet at gate → warp → position updated to destination ✅
- Unit: JumpgateSystem — ship out of range → warp fails ✅
- Unit: JumpgateSystem — inactive gate rejects warp ✅
- Integration: All systems tick 100 frames without crash ✅
- Integration: Ship moves over multiple frames with monotonic progress ✅

> **Performance Guidance — EventManager**: The existing `EventManager` uses `std::scoped_lock` on every `Publish`/`Subscribe`/`Unsubscribe` call, which serializes all event dispatch. **Do not use `EventManager` for hot-path simulation systems** (movement, collision, combat). These systems should use direct function calls or per-system callback lists. Reserve `EventManager` for infrequent events only (player login, chat, docking, session management).

---

### Phase 6 — Client Game Loop & World Sync ✅ COMPLETED

**Goal**: The client connects to the server, receives world state snapshots, and renders the live game world with first-person camera and fleet command controls.

#### Deliverables

| # | Task | Project | Files | Status |
|---|---|---|---|---|
| 6.1 | **Client state manager** — maintain a local mirror of server entities; apply state updates, interpolate between snapshots | Earthrise | `ClientWorldState.h/.cpp` | ✅ |
| 6.2 | **Entity interpolation** — smooth rendering between server ticks using timestamp-based lerp | Earthrise | `Interpolation.h` (header-only) | ✅ |
| 6.3 | **Client-side prediction** — immediately show owned fleet movement toward command target, reconcile with server corrections | Earthrise | `Prediction.h/.cpp` | ✅ |
| 6.4 | **Input state manager** — abstraction layer between raw Windows messages and game actions. Captures per-frame input state (key down/up/held, mouse position/delta, scroll wheel, touch input). Provides polling API (`IsKeyDown`, `GetMouseDelta`, `GetTouchPoints`) and maps raw input to configurable abstract actions (`InputAction::MoveForward`, `InputAction::SelectUnit`, `InputAction::ZoomTactical`). **Keyboard + mouse is the primary input method** for this genre; touch is supported as a secondary input for tablet/hybrid devices. Use `WM_POINTER` messages (available on all targeted Windows 10+ versions) for touch/pen input rather than `WM_TOUCH`/`WM_GESTURE`. Touch gestures: tap = select, drag = camera pan, pinch = zoom, long-press = context menu | Earthrise | `InputState.h/.cpp` | ✅ |
| 6.5 | **Input handler** — consumes `InputState` to produce game-level commands: first-person flight controls (WASD/mouse for flagship); fleet command overlay (right-click move-to via 3D ray cast, attack-target, group hotkeys); touch alternatives (tap-select, drag-move); send commands to server via UDP | Earthrise | `InputHandler.h/.cpp` | ✅ |
| 6.6 | **Connect flow** — startup → connect to server → login → receive initial zone state → begin rendering. `GameApp::Startup()` uses `co_await` for async operations (leveraging `ASyncLoader` base class). **Note**: never call `ASyncLoader::WaitForLoad()` on the main thread — it busy-spins with `std::this_thread::yield()`. Use `co_await` exclusively for async waits in coroutine contexts | Earthrise | `GameApp.cpp` expansion | ✅ |
| 6.7 | **Render active world** — iterate `ClientWorldState` entities, dispatch to appropriate category renderer | Earthrise | `GameApp::RenderScene()` | ✅ |
| 6.8 | **Fleet selection UI** — multi-select (click, Ctrl+click, box-drag in tactical view), selection brackets around owned ships, group assignment (Ctrl+1..9), move/attack cursors | Earthrise | `FleetSelectionUI.h/.cpp` | ✅ |
| 6.9 | **3D command targeting** — ray-cast from camera through mouse position into 3D space; use tactical grid plane or target object for move/attack destination | Earthrise | `CommandTargeting.h/.cpp` | ✅ |

#### Tests (Phase 6)

| Test | Status |
|---|---|
| Unit: Interpolation — lerp at t=0, t=0.5, t=1 gives correct positions; slerp quaternion midpoint | ✅ 6 tests |
| Unit: Prediction — apply input locally, update moves toward target, reconcile snap/blend, arrival clears | ✅ 5 tests |
| Unit: ClientWorldState — spawn creates entity, despawn removes, snapshot updates targets, multiple entities tracked | ✅ 5 tests |
| Unit: InputState — key press/release tracking, mouse delta computation | ✅ 3 tests |
| Unit: 3D ray-cast — NDC conversion, ray-plane intersection, ray-sphere intersection (hit and miss) | ✅ 7 tests |
| Unit: ComputeAlpha — alpha at t=0, t=0.5, t=1, clamped at max | ✅ 4 tests |

**Total: 30 unit tests, all passing** (`EarthRiseTests/ClientSyncTests.cpp`)

---

### Phase 7 — Gameplay Systems ✅ COMPLETED

**Goal**: Implement the core fleet-command gameplay mechanics that make Earthrise playable.

> **Status**: All deliverables implemented. CombatSystem (attack targets, turret auto-fire, projectile spawning, damage via shield→armor→hull pipeline, ship destruction with crate drop), ProjectileSystem (lifetime management, despawn on hit/expire), LootSystem (ship-crate collision → loot pickup event), DockingSystem (ship-station proximity → dock event), NpcAI (6-state machine: Idle/Patrol/Aggro/Chase/Attack/Return with faction-based hostility scanning), FleetSystem (fleet composition, formation movement with Line/Wedge/Sphere types), Fleet.h and ShipLoadout.h shared types. CommandProcessor now dispatches AttackTarget, Dock, and Loot commands. Zone tick orchestrates all systems. 32 new unit tests, 146 total passing. Both Debug and Release x64 builds pass.

#### Deliverables

| # | Task | Project | Files | Status |
|---|---|---|---|---|
| 7.1 | **Combat system** — fleet attack commands (attack-target, attack-move, focus fire), turret auto-fire within range, projectile spawning, hit detection, damage application, ship destruction | GameLogic | `CombatSystem.h/.cpp` | ✅ Done |
| 7.2 | **Ship loadout** — hull type, mounted turrets, shield/armor/hull HP, energy system | NeuronCore (types) + GameLogic (logic) | `GameTypes/ShipLoadout.h` | ✅ Done |
| 7.3 | **Projectile lifecycle** — spawn from turret, travel, collide, despawn on hit or timeout | GameLogic | `ProjectileSystem.h/.cpp` | ✅ Done |
| 7.4 | **Crate looting** — any fleet ship enters proximity → loot pickup event → player inventory update | GameLogic | `LootSystem.h/.cpp` | ✅ Done |
| 7.5 | **Station docking** — enter docking range → server event → client opens station UI | GameLogic | `DockingSystem.h/.cpp` | ✅ Done |
| 7.6 | **NPC / AI fleets** — patrol waypoints in 3D, aggro radius, chase, attack state machine; group/formation behavior for NPC squads | GameLogic | `NpcAI.h/.cpp` | ✅ Done |
| 7.7 | **Data-driven tuning** — combat numbers defined in ObjectDefs (ShipDef, TurretDef, ProjectileDef); CombatSystem reads turret stats at runtime. Binary data file loading deferred to Phase 8+ when persistence is needed | GameLogic | — | ✅ Done (via ObjectDefs) |
| 7.8 | **Fleet management** — player fleet composition (which ships the player owns/commands), formation movement, ship acquisition | GameLogic + NeuronCore (types) | `FleetSystem.h/.cpp`, `GameTypes/Fleet.h` | ✅ Done |

#### Tests (Phase 7)

| Test | Status |
|---|---|
| Unit: CombatSystem — projectile hits ship → shield HP reduced by expected damage | ✅ |
| Unit: CombatSystem — damage overflows from shield to armor correctly | ✅ |
| Unit: CombatSystem — ship destroyed when hull HP reaches zero | ✅ |
| Unit: CombatSystem — friendly fire (source == target ship) is ignored | ✅ |
| Unit: CombatSystem — turret fires when target is in range | ✅ |
| Unit: ProjectileSystem — projectile despawns after lifetime expires | ✅ |
| Unit: ProjectileSystem — projectile travels at speed over time | ✅ |
| Unit: ProjectileSystem — DespawnProjectiles removes hit projectiles | ✅ |
| Unit: LootSystem — ship-crate collision generates loot event and destroys crate | ✅ |
| Unit: LootSystem — non-ship-crate collision is ignored | ✅ |
| Unit: DockingSystem — ship in docking range generates dock event | ✅ |
| Unit: DockingSystem — ship outside docking range produces no event | ✅ |
| Unit: DockingSystem — direct range check works | ✅ |
| Unit: NPC AI — NPC starts in Patrol state with waypoints | ✅ |
| Unit: NPC AI — NPC transitions to Aggro on detecting hostile | ✅ |
| Unit: NPC AI — NPC transitions from Aggro to Chase | ✅ |
| Unit: NPC AI — NPC transitions to Attack when in range | ✅ |
| Unit: NPC AI — NPC returns when target destroyed | ✅ |
| Unit: NPC AI — Idle NPC with no waypoints scans periodically | ✅ |
| Unit: Fleet data — add/remove ship, capacity enforced | ✅ (3 tests) |
| Unit: Fleet data — formation offset leader at center | ✅ |
| Unit: Fleet data — formation line spacing correct | ✅ |
| Unit: Fleet data — formation wedge symmetric | ✅ |
| Unit: FleetSystem — create fleet and add ships | ✅ |
| Unit: FleetSystem — remove ship from fleet | ✅ |
| Unit: FleetSystem — remove ship from all fleets | ✅ |
| Unit: FleetSystem — formation move sets targets | ✅ |
| Unit: FleetSystem — set formation type | ✅ |
| Unit: FleetSystem — remove fleet cleans up | ✅ |
| Integration: Two fleets engage in combat → ships take damage | ✅ |
| Integration: All systems tick 100 frames without crash | ✅ |

**Total: 32 new unit tests (146 total), all passing** (`EarthRiseTests/GameplayTests.cpp`)

---

### Phase 8 — HUD, UI & Player Interaction ✅ COMPLETED

**Goal**: Player-facing interface elements for first-person flight + fleet command.

#### Deliverables

| # | Task | Project | Files | Status |
|---|---|---|---|---|
| 8.1 | **HUD overlay** — flagship HP/shields/armor/energy/speed bars (bottom-left), target info panel (top-right), crosshair. All Darwinia-style flat-color bars via `SpriteBatch`. Pure computation in `HUDLayout` (static methods, unit-testable) | Earthrise | `HUD.h/.cpp` | ✅ Done |
| 8.2 | **Targeting system** — Tab-cycle, Shift+Tab reverse-cycle, T for nearest target. Target brackets (red corner brackets) rendered in 3D→screen projection. Filter by category (ships, stations, asteroids). Sorted target list for stable cycling | Earthrise | `TargetingUI.h/.cpp` | ✅ Done |
| 8.3 | **Chat** — ring buffer (128 messages), 8 visible, 10s fade. Typing state machine (Enter toggles, printable chars, Backspace, Escape cancels). Channel-colored bars (Zone/Party/Whisper/System). Cursor indicator when typing | Earthrise | `ChatUI.h/.cpp` | ✅ Done |
| 8.4 | **Main menu** — deferred (requires text rendering not yet available via SpriteBatch) | Earthrise | — | ⏳ Deferred |
| 8.5 | **Station UI** — deferred (requires text rendering for trade/fitting UI) | Earthrise | — | ⏳ Deferred |
| 8.6 | **Settings** — deferred (requires text rendering for labels and inputs) | Earthrise | — | ⏳ Deferred |
| 8.7 | **Rendering**: All HUD/UI rendered via `GameApp::RenderCanvas()` composited on the 3D scene. UI uses `SpriteBatch` for solid-color quads — Darwinia-style minimal chrome (bars, brackets, indicators). New `InputAction` values wired in `InputState`/`InputHandler` | Earthrise | `GameApp.cpp`, `InputState.cpp`, `InputHandler.cpp` | ✅ Done |
| 8.8 | **Fleet command panel** — ship list with hull/shield bars on left side. `ComputeLayout()` returns testable `FleetPanelEntry` structs. Hull bar uses red→green gradient by fraction | Earthrise | `FleetPanel.h/.cpp` | ✅ Done |
| 8.9 | **Jumpgate UI** — proximity detection (100m range), warp-available indicator (blue bordered square with diamond) in bottom-right corner. `FindNearbyJumpgate()` selects nearest | Earthrise | `JumpgateUI.h/.cpp` | ✅ Done |
| 8.10 | **Ship status broadcast** — `ShipStatusMsg` (46 bytes/entry, batched 30/packet) sent each tick. `ShipStatusEntry` carries shield/armor/hull/energy/speed/faction. `PlayerInfoMsg` sent once after login to identify flagship | NeuronCore, EarthRiseServer | `Messages.h`, `StateBroadcaster.cpp`, `ServerLoop.cpp` | ✅ Done |
| 8.11 | **Client entity stats** — `ClientEntity` extended with 11 ship stat fields. `ApplyShipStatus()` and `SetFlagship()`/`GetFlagship()` on `ClientWorldState` | Earthrise | `ClientWorldState.h/.cpp` | ✅ Done |

#### Tests (Phase 8) — 42 new tests (188 total, all passing)

| Test Class | Count | Coverage |
|---|---|---|
| `HUDLayoutTests` | 10 | ComputeBar fractions (half/full/empty/zero-max/over/negative), ShieldBar position, TargetBars top-right, all player bars at different vertical positions |
| `TargetingUITests` | 7 | RefreshTargetList (finds ships, respects filter), CycleNext wrapping, CyclePrev wrapping, SetTarget index, ClearTarget reset, empty list |
| `ChatUITests` | 13 | AddMessage store + trim-to-max, typing toggle, TypeChar append/reject/max-length/non-printable, Backspace remove/empty, CommitInput, GetVisibleMessages recent/faded/typing-mode |
| `FleetPanelTests` | 4 | ComputeLayout entry count, hull/shield fractions, vertical stacking, empty fleet |
| `JumpgateUITests` | 4 | FindNearbyJumpgate in-range/out-of-range/selects-nearest, Update sets warp available |
| `ShipStatusMsgTests` | 3 | ShipStatusMsg round-trip serialization, PlayerInfoMsg round-trip, ApplyShipStatus updates ClientEntity |
| `FlagshipTests` | 2 | Set/Get flagship, default is invalid |

---

### Phase 9 — Audio & Effects ✅ COMPLETED

**Goal**: Spatial audio and visual effects for an immersive experience.

> **Status**: All deliverables implemented. Game event types defined in NeuronCore for pub/sub. SoundBank catalogs 9 sound effects. SpatialAudioSystem wraps XAudio2/X3DAudio for 3D positioned audio with silent-mode fallback. ParticleSystem provides CPU-side pool (4096 max) with velocity integration, alpha fade, and Xorshift32 RNG. ParticleRenderer draws flat-color particles through FlatColorPipeline. AudioEventHandler bridges EventManager events to audio + particle spawning. 45 new unit tests (233 total, all passing). Debug and Release x64 builds verified.

#### Deliverables

| # | Task | Project | Files | Status |
|---|---|---|---|---|
| 9.1 | **Game events** — 5 event types (`ExplosionEvent`, `ProjectileHitEvent`, `ShieldHitEvent`, `DockEvent`, `EngineEvent`) deriving from `Event` base, published via `EventManager` | NeuronCore | `GameEvents.h` | ✅ Done |
| 9.2 | **Sound bank** — catalog of 9 game sounds (`SoundId` enum: EngineHum, WeaponFire, Explosion, ShieldHit, AmbientSpace, DockingClamp, UndockRelease, ProjectileLaunch, TargetLock) with volume, distance, and looping parameters | Earthrise | `SoundBank.h/.cpp` | ✅ Done |
| 9.3 | **Spatial audio integration** — 3D positioned sounds using existing `AudioEngine` X3DAudio support. `UpdateListener()` tracks camera with velocity. `PlayOneShot()` for 3D positioned sounds, `PlayUI()` for non-positional. Silent-mode fallback when `AudioEngine` is nullptr | Earthrise | `SpatialAudioSystem.h/.cpp` | ✅ Done |
| 9.4 | **Particle effects** — CPU-side particle pool (4096 max). Velocity integration, age/lifetime tracking, linear alpha fade, expired removal. `SpawnBurst()` creates spherical random spread (theta/phi) with Xorshift32 RNG, random lifetimes [0.3–1.2s], random sizes. Rendered via `FlatColorPipeline` with POINTLIST topology, self-lit (AmbientIntensity=1.0), origin-rebased positions | GameRender | `ParticleSystem.h/.cpp`, `ParticleRenderer.h/.cpp` | ✅ Done |
| 9.5 | **Event-driven audio** — `AudioEventHandler` subscribes to all 5 game events via `EventManager`. Explosion → 32 orange particles + sound. ProjectileHit → 8 yellow particles + sound. ShieldHit → 12 cyan particles + sound. Dock → UI sound. Engine → 4 blue exhaust particles. Counters for test observability | Earthrise | `AudioEventHandler.h/.cpp` | ✅ Done |
| 9.6 | **GameApp integration** — All systems wired into `GameApp`: spatial audio initialized (silent mode), particles updated in `Update()`, particles rendered after world entities in `RenderScene()`, `EntityDespawn` publishes `ExplosionEvent` | Earthrise | `GameApp.h/.cpp` | ✅ Done |

#### Tests (Phase 9) — 45 new tests (233 total, all passing)

| Test Class | Count | Coverage |
|---|---|---|
| `SoundBankTests` | 10 | Default catalog has all 9 sounds, contains specific entries (EngineHum/WeaponFire/Explosion/ShieldHit), looping flags correct, overwrite entry, invalid ID returns nullptr, distances valid, IDs unique |
| `ParticleSystemTests` | 13 | Initially empty, spawn burst creates particles, spawn single, position integration, aging, expired removal, alpha fade, max particles cap (4096), clear, velocities nonzero, lifetimes vary, gradual removal |
| `GameEventsTests` | 6 | Publish/subscribe for each event type (Explosion, ProjectileHit, ShieldHit, Dock, Engine), SetConsumed stops handler chain |
| `AudioEventHandlerTests` | 11 | Subscribe verification, counter increments for each event type, particle spawning from explosion/hit/shield/engine events, engine inactive spawns no particles |
| `SpatialAudioSystemTests` | 5 | Silent mode initialization, not loaded by default, PlayOneShot/PlayUI no-throw in silent mode, preload silent no-throw, UpdateListener no-throw |

---

### Phase 10 — Local Server Deployment & Database ✅ COMPLETED

**Goal**: Package `EarthRiseServer` in a Docker container, persist game data in a local MSSQL instance, and validate the full stack with Docker Desktop.

> **Status**: All deliverables implemented. ServerConfig reads `.env` files and environment variable overrides. HealthCheck provides TCP health endpoint on configurable port with background thread. IDatabase interface with MockDatabase (tests) and SqlDatabase (ODBC/MSSQL). AuthSystem uses PBKDF2-SHA256 via Windows BCrypt API for password hashing with random salts, constant-time verification, and cryptographic session tokens. Telemetry outputs structured JSON log entries to stdout with configurable output callback. Dockerfile.server (Windows Server Core), docker-compose.yml (MSSQL 2022 + schema init), CI/CD pipeline (GitHub Actions), and MSSQL schema (Accounts, Ships, Inventory, WorldState) all created. ServerLoop integrated with config-driven startup. 49 new unit tests (282 total, all passing). Debug and Release x64 builds verified.

#### Deliverables

| # | Task | Location | Files | Status |
|---|---|---|---|---|
| 10.1 | **Dockerfile** — Windows Server Core container image with statically linked EarthRiseServer.exe. HEALTHCHECK via TCP probe to health port. Exposes UDP 7777 (game) and TCP 8080 (health) | repo root | `Dockerfile.server` | ✅ Done |
| 10.2 | **Docker Compose** — MSSQL 2022 Linux container with health check + `db-init` service runs `schema.sql` after MSSQL is healthy. Server runs natively on host connecting to localhost:1433 | repo root | `docker-compose.yml` | ✅ Done |
| 10.3 | **Health probes** — TCP health endpoint on configurable port (default 8080). Background thread with `select()` + 100ms timeout for clean shutdown. Responds HTTP 200 to any connection | EarthRiseServer | `HealthCheck.h/.cpp` | ✅ Done |
| 10.4 | **Configuration** — `ServerConfig::Load()` reads `.env` file then environment variable overrides (EARTHRISE_PORT, EARTHRISE_TICK_RATE, EARTHRISE_MAX_CLIENTS, EARTHRISE_HEALTH_PORT, EARTHRISE_DB_CONNECTION_STRING, EARTHRISE_ZONE_FILE, EARTHRISE_LOG_FORMAT). Case-insensitive key matching, value validation | EarthRiseServer | `ServerConfig.h/.cpp` | ✅ Done |
| 10.5 | **Persistent storage** — `IDatabase` abstract interface with `Connect`/`Disconnect`/`CreateAccount`/`FindAccount`/`UpdateLastLogin`. `SqlDatabase` implements via ODBC with prepared statements, unique constraint violation detection. `AccountRecord` struct for data transfer | EarthRiseServer | `Database.h`, `SqlDatabase.h/.cpp` | ✅ Done |
| 10.6 | **CI/CD pipeline** — GitHub Actions: checkout → vcpkg → CMake configure (x64-release) → build → CTest → upload server artifact → Docker image build on main branch | `.github/workflows/` | `build-deploy.yml` | ✅ Done |
| 10.7 | **Monitoring** — Structured JSON logging (`Telemetry::LogJson`/`LogText`). ISO 8601 timestamps with milliseconds, log levels (Info/Warning/Error), JSON string escaping, configurable output callback for testing. Integrated into ServerLoop startup/shutdown | EarthRiseServer | `Telemetry.h/.cpp` | ✅ Done |
| 10.8 | **Database schema** — MSSQL: `Accounts` (Id, Username UNIQUE, PasswordHash, Salt, CreatedAt, LastLogin), `Ships` (AccountId FK, ShipDefIndex, ShipName, IsFlagship), `Inventory` (AccountId FK, ItemType, Quantity, Metadata JSON), `WorldState` (ZoneId, Category, DefIndex, position, rotation). Idempotent `IF OBJECT_ID IS NULL` creation | `deploy/sql/` | `schema.sql` | ✅ Done |
| 10.9 | **Custom authentication** — PBKDF2-SHA256 (100K iterations) via Windows BCrypt API. 16-byte random salt, 32-byte derived key, hex encoding. Constant-time password verification (timing-attack resistant). 32-byte cryptographic session tokens. `.env.example` with sample config | EarthRiseServer | `AuthSystem.h/.cpp`, `.env.example` | ✅ Done |
| 10.10 | **ServerLoop integration** — `Startup(const ServerConfig&)` overload uses config-driven port, tick rate, health port. Telemetry JSON logging at startup/shutdown. HealthCheck started/stopped automatically. Legacy `Startup(port)` overload preserved for backward compatibility | EarthRiseServer | `ServerLoop.h/.cpp`, `WinMain.cpp` | ✅ Done |

#### Tests (Phase 10) — 49 new tests (282 total, all passing)

| Test Class | Count | Coverage |
|---|---|---|
| `ServerConfigTests` | 14 | Default values (port/tickRate/maxClients/healthPort/logFormat), ParseEnvLine simple/comment/blank/quoted/whitespace, ApplyKeyValue port/tickRate/case-insensitive/invalid-ignored |
| `AuthSystemTests` | 13 | GenerateSalt non-empty/correct-length/unique, HashPassword non-empty/correct-length/deterministic/different-salts, VerifyPassword correct/wrong, GenerateSessionToken non-empty/correct-length/unique, BytesToHex↔HexToBytes round-trip |
| `TelemetryTests` | 10 | FormatJson has timestamp/level/message/extra-fields, escape quotes/newlines/control-chars, LevelToString values, GetTimestamp ISO format, OutputCallback captures log |
| `DatabaseMockTests` | 8 | Connect/Disconnect state, CreateAccount stores, FindAccount retrieves/not-found, DuplicateKey detection, UpdateLastLogin modifies, IDatabase polymorphism |
| `HealthCheckTests` | 4 | NotRunning by default, Start sets running, Stop clears running, TCP connection receives HTTP 200 |

---

### Phase 11 — Polish, Optimization & Ship Readiness

**Goal**: Performance tuning, bug fixing, and final quality pass.

#### Deliverables

| # | Task | Notes |
|---|---|---|
| 11.1 | **Frame-time profiling** — PIX captures on client, identify bottlenecks | Use existing `PixProfiler.h` |
| 11.2 | **Server tick profiling** — measure per-system tick times, ensure budget met | Custom server-side timing |
| 11.3 | **Memory audit** — verify no leaks (CRT debug heap on client, custom tracking on server) | `_CrtSetDbgFlag` already in `WinMain.cpp` |
| 11.4 | **LOD system** — reduce draw calls for distant objects, simplified collision for far entities | GameRender, GameLogic |
| 11.5 | **Frustum culling** — skip rendering off-screen objects | GameRender (Camera) |
| 11.6 | **Parallel command list recording** — `GraphicsCore` currently has a single `m_commandList`; if CPU-side serial recording becomes a bottleneck, add per-thread command lists with `ExecuteBundle` or deferred command lists for parallel scene submission | GameRender, NeuronClient |
| 11.7 | **Bandwidth optimization** — delta compression, area-of-interest filtering, dead reckoning | NeuronCore, EarthRiseServer |
| 11.8 | **Stress testing** — simulated 100+ clients, verify server stability | Test tooling |
| 11.9 | **Final build verification** — Debug + Release × all CMake presets (Ninja, VS generator) | CI pipeline |

---

## 7. Testing Strategy

### Unit Tests (EarthRiseTests project)

**Framework**: Visual Studio Native Unit Test Framework (`CppUnitTest.h`)

| Area | What We Test | Phase |
|---|---|---|
| NeuronCore Math | Vector operations, matrix transforms, alignment utilities | 1 |
| NeuronCore Serialization | DataReader/DataWriter round-trip for all types + strings | 1 |
| NeuronCore FileSys / BinaryFile | `BinaryFile::WriteFile`/`ReadFile` round-trip + `BinaryDataReader`/`BinaryDataWriter` structured serialization for arbitrary-length data; no 1400-byte limit | 1 |
| NeuronCore ENUM_HELPER | Postfix `++` returns pre-increment value; iterator range covers all values | 1 |
| NeuronCore Events | Subscribe, publish, unsubscribe, multi-subscriber ordering | 1 |
| NeuronCore NetLib | Packet framing, message ID dispatch | 4 |
| GameLogic Handles | EntityHandle create, validate, invalidate, generation wrap | 2 |
| GameLogic Pools | EntityPool alloc, free, lookup, exhaustion | 2 |
| GameLogic SpaceObjects | Create/destroy each category, property access | 2 |
| GameLogic Movement | Velocity integration, rotation, clamping | 5 |
| GameLogic Collision | Sphere-sphere, spatial hash insert/query | 5 |
| GameLogic Combat | Damage calculation, destruction threshold | 7 |
| GameLogic AI | State machine transitions | 7 |
| GameRender CMO | Parse real game meshes (`Asteroid01.cmo`, `HullShuttle.cmo`); batch-validate all 65 `.cmo` files load without error | 3B |
| GameRender MeshCache | Deduplication, handle validity, `PreloadCategory` loads all meshes in a subfolder | 3B |
| GameRender GPU Infra | Upload heap ring buffer, constant buffer alignment | 3A |
| InputState | Key state polling, touch → action mapping | 6 |
| Interpolation | Lerp correctness at boundary and mid values | 6 |
| Prediction | Input apply + server reconciliation | 6 |

### Integration Tests

| Test | What It Validates | Phase |
|---|---|---|
| Loopback handshake | Client connects to server on localhost, login succeeds | 4 |
| Entity sync | Server spawns entity → client receives and creates local mirror | 6 |
| Fleet command round-trip | Player issues move command → server processes → fleet ships move → client sees interpolated positions | 6 |
| Combat round-trip | Player attack-commands target → server processes → projectile spawns → target takes damage → client sees HP update | 7 |
| Jumpgate warp | Player fleet at jumpgate → warp command → fleet arrives at destination gate → client renders at new location | 7 |
| Container boot | Docker image starts, health probe passes | 10 |
| Custom auth flow | Register new account → login → session created → reconnect with token | 10 |
| Database round-trip | Server saves player state to Azure SQL → restarts → player state restored | 10 |
| Multi-client stress | N simulated clients, server tick rate stays above threshold | 10 |

### Test Data

- **CMO meshes**: Use the 65 existing game meshes under `Earthrise/Assets/Mesh/<type>/*.cmo` for rendering tests — no synthetic test meshes needed. Representative test set: `Asteroids/Asteroid01.cmo` (simple), `Hulls/HullShuttle.cmo` (multi-submesh hull), `Stations/Mining01.cmo` (large station), `SpaceObjects/DebrisGenericBarrel01.cmo` (generic base category)
- Deterministic zone definition file for repeatable server tests
- Recorded input sequences for replay tests

---

## 8. Risk Register

| Risk | Impact | Likelihood | Mitigation |
|---|---|---|---|
| CMO meshes have no useful material data for colors | Rendering blocked | Low | **Confirmed**: colors assigned at runtime from object definitions, not from CMO materials — CMO only provides geometry. All 65 existing meshes under `Assets/Mesh/` are geometry-only |
| CMO mesh variety across categories | Some meshes may have unexpected submesh configurations or vertex formats | Medium | Phase 3B batch-validates all 65 `.cmo` files during testing; loader must handle varying submesh counts and vertex layouts gracefully. **Validate vertex stride diversity early**: write a scan tool that reads the vertex stride from each `.cmo` header to confirm layout consistency. Some meshes may contain skinning data that the loader must strip or handle |
| UDP reliability layer complexity | Networking delayed | Medium | Start with simple sequence/ACK; only login/chat/inventory need reliability; state updates are fire-and-forget |
| UDP packet loss causes jerky movement | Visual quality | Medium | Entity interpolation + dead reckoning on client; redundant state in each snapshot |
| Windows containers on AKS limited support | Deployment blocked | Medium | Evaluate Windows node pools early; server targets Windows Server Core containers exclusively |
| C++23 feature support varies between compiler versions | Build breaks on different machines | Low | CMake enforces `CMAKE_CXX_STANDARD 23` with `CMAKE_CXX_STANDARD_REQUIRED ON`; pin to a known MSVC toolset version if needed |
| Server tick rate drops under load | Gameplay quality | High | Profile early (Phase 5), budget per-system, amortize AI across frames |
| No existing test infrastructure | Bugs ship undetected | Medium | Phase 1 creates test project; require tests for every phase |
| `NetLib.h` doesn't exist yet; DataReader/DataWriter won't compile | Build broken | High | Phase 1 priority — create `NetLib.h` first |
| Azure SQL latency for real-time saves | Player data loss on crash | Medium | Batch writes; save on dock/logout/timer, not per-tick; Redis for hot session state |
| Single-zone scalability ceiling | Performance at high player counts | High | Spatial partitioning for updates; area-of-interest filtering; profile early in Phase 5 |
| Tactical zoom reveals many entities at once | Higher render/network cost at wide zoom | Medium | Aggressive frustum culling; LOD based on camera distance; bandwidth throttling per AoI radius |
| First-person + fleet command is complex UX | Steep learning curve; input conflicts | Medium | Clear mode separation (flight vs. command); keyboard+mouse primary with touch as secondary fallback; tutorial flow; test with real players early |
| Full 3D movement + 3D spatial hash | More complex collision/partitioning than 2D | Low | Well-tested 3D spatial hash (hash-mapped cells, only occupied cells allocated); sphere-based broad phase keeps it simple |
| DX12 GPU infrastructure gaps | Rendering blocked; no upload heap, constant buffer management, or PSO helpers exist | High | Phase 3A creates all GPU infrastructure before any rendering code; verify with D3D12 debug layer |
| Z-fighting at large draw distances | Visual artifacts on distant objects | Medium | Reverse-Z depth buffer (Phase 3A.4) provides near-logarithmic precision distribution |
| `RenderCanvas()` never called in main loop | All Phase 8 HUD/UI renders to nothing | High | Phase 1.9 adds the call to `WinMain.cpp`; verify early with a test overlay |
| Touch + mouse/keyboard input complexity | Three input sources increase testing surface | Medium | `InputState` abstraction layer normalizes all inputs; keyboard+mouse is primary, touch is secondary; test each independently |
| `float` precision at 100 km zone edges | Projectile hit detection jitter, docking inaccuracy at distant stations (~8 mm precision at 100,000 units) | Medium | Server stores positions as `double` (or periodic origin-rebase); client uses camera-relative origin-rebasing (Phase 3C.4) for GPU-side rendering; state broadcaster sends positions relative to player camera |
| NeuronCore WinRT dependency breaks server containers | `NeuronCore.h` includes WinRT projections; `TasksCore.h` uses WinRT thread pool timers; Windows Server Core may not have these APIs | High | Phase 1.13 splits NeuronCore headers: `NeuronCoreBase.h` (server-safe) vs. `NeuronCoreWinRT.h` (client-only); `TasksCore` gets Win32 `CreateTimerQueueTimer` fallback |
| No shader-visible descriptor heap management | Bloom post-processing (Phase 3D) and future SRV-based effects cannot bind resources to shaders | High | Phase 3A.5 creates shader-visible descriptor heap ring allocator before any post-processing code |
| `SpriteBatch` missing for Phase 8 UI | HUD/UI rendering has no sprite batcher to draw text or icons | Medium | Phase 3D.3 adds `SpriteBatch` (either custom or DirectXTK12 via vcpkg) before Phase 8 begins |
| `ASyncLoader::WaitForLoad()` busy-spins | Calling on main thread burns a full CPU core doing nothing | Low | Never call `WaitForLoad()` from main thread; use `co_await` in `GameApp::Startup()` coroutine; document this constraint (Phase 6.6) |
| `DataReader`/`DataWriter` hardcoded to 1400 bytes | Zone definition files and large data sets cannot be loaded via these classes | High | Phase 1.12 extends `BinaryFile` with `WriteFile` and adds `BinaryDataReader`/`BinaryDataWriter` helpers over `byte_buffer_t` for disk I/O; `DataReader`/`DataWriter` remain network-only |
| No GPU default-heap management for static geometry | Vertex/index buffers remain in upload heap (CPU-visible, slow GPU reads) instead of being promoted to default heap | Medium | Phase 3A.6 adds `GpuResourceManager` for upload → default-heap copy; mesh loading uses this for all static buffers |

---

## 9. Conventions & Standards

All code follows the established codebase conventions documented in `.github/copilot-instructions.md`:

| Rule | Example |
|---|---|
| Files: `PascalCase.h` / `PascalCase.cpp` | `EntityPool.h`, `ShipRenderer.cpp`, `GameTypes/SpaceObject.h` |
| Classes/Functions: `PascalCase` | `class SpaceObjectManager`, `void ProcessInputs()` |
| Members: `m_` prefix | `m_position`, `m_velocity` |
| Globals: `g_` prefix | `g_app` |
| Constants: `UPPER_SNAKE_CASE` | `MAX_ENTITIES`, `DATALOAD_SIZE` |
| Assertions: `ASSERT`, `DEBUG_ASSERT` | `ASSERT(handle.IsValid())` |
| Logging: `Neuron::DebugTrace` | `DebugTrace("Entity {} spawned\n", id)` |
| Fatal: `Neuron::Fatal` | `Neuron::Fatal("Pool exhausted")` |
| COM pointers: `winrt::com_ptr` | `com_ptr<ID3D12Resource>` |
| SIMD vectors as params: `FXMVECTOR` + `XM_CALLCONV` | `void XM_CALLCONV SetPosition(FXMVECTOR _pos)` |
| Storage vectors: `XMFLOAT3`, `GameVector3` | struct member `XMFLOAT3 m_position` |
| PCH: every `.cpp` starts with `#include "pch.h"` | — |
| New renderers: GameRender project, not NeuronClient | `GameRender/ShipRenderer.h` |
| Shared data types: NeuronCore/GameTypes/ | `NeuronCore/GameTypes/EntityHandle.h`, `NeuronCore/GameTypes/SpaceObject.h` |
| Simulation systems: GameLogic (server-only) | `GameLogic/MovementSystem.h`, `GameLogic/CombatSystem.h` |
| NeuronCore: modern C++ (`std::string_view`, `constexpr`, `[[nodiscard]]`, `noexcept`) | — |
| Legacy projects (NeuronClient): raw pointers, C-style strings where existing patterns dictate | — |
| GameLogic (new server code): modern C++ like NeuronCore | — |
| Enum iteration: use `ENUM_HELPER` from `NeuronHelper.h` | `ENUM_HELPER(SpaceObjectCategory, SpaceObject, Turret)` |
| Flag enums: use `ENUM_FLAGS_HELPER` for bitwise operators | `ENUM_FLAGS_HELPER(ShipFlags, Engine, Shields)` |
| Tests: Visual Studio Native Unit Test Framework | `TEST_CLASS`, `TEST_METHOD` |
| Coroutine startup: `GameApp::Startup()` returns `IAsyncAction` | Use `co_await` for async loading; leverage `ASyncLoader` base class |
| Input: **keyboard+mouse primary**, touch supported | `InputState` abstracts all input sources into polling API + action mapping; `WM_POINTER` for touch/pen |
| Data files (network): `DataReader`/`DataWriter` | Network packets only; fixed 1400-byte buffer (`DATALOAD_SIZE`). Do not use for file I/O |
| Data files (disk): `BinaryFile` + `BinaryDataReader`/`BinaryDataWriter` | `BinaryFile::ReadFile`/`WriteFile` for raw I/O; `BinaryDataReader`/`BinaryDataWriter` wrap `byte_buffer_t` with cursor-based `Read<T>`/`Write<T>` via CRTP `SerializationBase`. No size limit; used for zone defs, object defs, tuning data |
| Depth buffer: **reverse-Z** | Near plane → large Z, far plane → 0; `D3D12_COMPARISON_FUNC_GREATER_EQUAL`; clear to `0.0f` |
| Rendering coordinates: **origin-rebased** | Subtract camera world position from entity positions before GPU submission; prevents `float` precision loss at zone edges |
| `EventManager`: infrequent events only | Do not use for hot-path simulation (movement, collision, combat); use direct calls or per-system callbacks instead |
| `ASyncLoader::WaitForLoad()`: avoid on main thread | Busy-spins with `yield()`; use `co_await` in coroutine contexts instead |
| Server headers: `NeuronCoreBase.h` | Server projects include the WinRT-free base header; client projects include full `NeuronCore.h` |
| Mesh assets: `Assets/Mesh/<CategoryFolder>/<MeshName>.cmo` | Folder per category (e.g., `Hulls/`, `Asteroids/`, `Stations/`); `Hulls/` maps to `Ship` category; mesh name (without `.cmo`) is the key in `MeshCache` and object definitions |
