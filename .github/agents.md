# AGENTS.md

## Project Overview

Earthrise is a Windows-native C++ MMO space game built with CMake 3.28+ and vcpkg. It targets Windows 10+ (x64) and ships as a Win32 desktop application. Rendering is Direct3D 12. The project contains multiple static libraries and two executables (client + server). The game follows a fully server-authoritative model with a Darwinia-inspired flat-color visual style.

**Current project structure (CMake):**

| Project | Type | Role |
|---|---|---|
| **NeuronCore** | Static lib | Engine foundation: math, timers, events, file I/O, async, tasks/threads, debug, data serialization, GameTypes (shared entity data types) |
| **NeuronClient** | Static lib | Client engine: DX12 graphics core, descriptor heaps, audio engine, sound effects, PIX profiling, windowing |
| **NeuronServer** | Static lib | Server engine: bootstrap, socket listener, session management |
| **Earthrise** | Win32 desktop application | Client executable: `GameApp`, main loop, input, HUD/UI |
| **EarthRiseServer** | Console application | Server executable: zone management, state broadcast, persistence |

**Planned projects (from MMO.md Phase 1):**

| Project | Type | Role |
|---|---|---|
| **GameLogic** | Static lib | Server-only simulation: movement, collision, combat, AI, spawning, fleet management |
| **GameRender** | Static lib | CMO mesh loader, flat-color pipeline, per-category renderers, camera, post-processing |
| **EarthRiseTests** | Native unit test | Unit + integration tests (VS Native Unit Test Framework) |

**Target dependency graph:**

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

## Key Architecture Decisions

- **Fully server-authoritative**: Server owns all simulation state. Client only renders.
- **GameLogic is server-only**: Simulation systems link only to EarthRiseServer. Client never has simulation code.
- **Shared data types in NeuronCore**: Entity data types (`SpaceObjectCategory`, `EntityHandle`, `SpaceObject` struct, per-category structs) live in `NeuronCore/GameTypes/`.
- **Visual style**: Darwinia-inspired flat-color rendering — no textures, runtime-assigned colors, bloom glow, low-poly CMO meshes.
- **Single zone**: One contiguous 100 km³ play area with jumpgate fast-travel.
- **First-person + fleet command**: Homeworld-style hybrid camera with tactical zoom.
- **Full 3D**: Ships move freely in all three axes.
- **UDP networking**: MTU-safe 1400-byte datagrams; reliable channel for login/chat/inventory, unreliable for state updates.
- **Azure SQL**: Player persistence (accounts, loadouts, inventory).

See [MMO.md](../MMO.md) for the full implementation plan with 11 phases.

## Documentation

| Document | Purpose |
|---|---|
| [README.md](../README.md) | Project overview and quick start |
| [MMO.md](../MMO.md) | Full MMO implementation plan (phases, architecture, taxonomy, conventions) |
| [coding-standards.md](coding-standards.md) | Naming, formatting, language conventions |

## Setup Commands

- Configure (Debug): `cmake --preset x64-debug`
- Build (Debug): `cmake --build --preset x64-debug`
- Configure (Release): `cmake --preset x64-release`
- Build (Release): `cmake --build --preset x64-release`
- Configure (VS generator): `cmake --preset vs-x64-debug` or `cmake --preset vs-x64-release`

## Development Workflow

- Configure and build via CMake presets (see Setup Commands above).
- Alternatively, use `cmake --preset vs-x64-debug` to generate a Visual Studio solution, then open the generated `.sln` from `out/build/vs-x64-debug/`.
- No hot reload/watch mode is configured.
- No required environment variables are defined.

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

## Testing Instructions

- **Framework**: Visual Studio Native Unit Test Framework (`CppUnitTest.h`)
- **Test project**: EarthRiseTests (planned — Phase 1.6)
- Manual validation is expected after rendering or gameplay changes.
- New utilities in NeuronCore may be covered by Visual Studio Native Unit Tests.

## Code Style

- Naming: files `PascalCase.cpp/.h`, classes `PascalCase`, methods `PascalCase`, members `m_` + `camelCase`, globals `g_` prefix, constants `UPPER_SNAKE_CASE`.
- Indentation: 2 spaces in app-level code; some libraries use tabs. Match the file.
- Include guards: use `#pragma once` (not `#ifndef`).
- Assertions/logging: `ASSERT`, `DEBUG_ASSERT`, `Neuron::DebugTrace`, `Neuron::Fatal`.
- PCH: all projects use `pch.h` / `pch.cpp`. New `.cpp` files must include `pch.h` first.
- Rendering: do not add new functionality to `ImRenderer`; use `SpriteBatch` for new textured-quad work.
- COM pointers: use `winrt::com_ptr<T>`, `.get()`, and `= nullptr` (not `Microsoft::WRL::ComPtr`, `.Get()`, `.Reset()`).
- Full standards in [coding-standards.md](coding-standards.md).

## Build and Deployment

- Build via CMake presets: `cmake --build --preset x64-debug` or `cmake --build --preset x64-release`.
- Use `Debug` for local dev and `Release` for shipping builds.
- Final executables output to `<root>/<Config>/<Platform>/` (e.g., `Debug/x64/Earthrise.exe`).
- Both Debug and Release x64 must build cleanly before merging.

## Pull Request Guidelines

- Title format: `[component] Brief description`
- Required checks: build `x64` in `Debug` and `Release`.
- Include a short summary of gameplay/graphics impact when applicable.

## DirectXMath / Vector Conventions

All vector and matrix math must use SIMD register types (`XMVECTOR`, `XMMATRIX`) for
computation. `XMFLOAT3` / `XMFLOAT4X4` are **storage only** — never perform
arithmetic on them. The load→compute→store boundary must be explicit.

| Context | Type | Reason |
|---|---|---|
| Struct/class members | `XMFLOAT3` / `XMFLOAT4X4` | Stable layout, serializable, no alignment requirement |
| Function parameters (non-virtual) | `FXMVECTOR` + `XM_CALLCONV` | SIMD register passing |
| Function parameters (virtual) | `const XMFLOAT3&` | Virtual dispatch cannot use `XM_CALLCONV` |
| Function return values | `XMVECTOR` or `XMFLOAT3` | `XMVECTOR` if caller will continue computing; `XMFLOAT3` if storing |
| Local temporaries in functions | `XMVECTOR` / `XMMATRIX` | **Always** — never `XMFLOAT3` for intermediate results |
| Loop bodies | `XMVECTOR` / `XMMATRIX` | Hoist loads before loop, store after |
| One-shot scalar queries | `XMFLOAT3` overload (e.g. `Length(XMFLOAT3)`) | Returns scalar — no vector kept in register |

> **Anti-pattern — do NOT add `XMFLOAT3` arithmetic operators** (`operator+`,
> `operator*`, etc.). They would hide a load→op→store per expression, defeating
> the entire purpose of the SIMD boundary. All vector arithmetic stays in `XMVECTOR`.

- **Parameter passing**: Always pass 3D vectors as `FXMVECTOR` (not `const XMFLOAT3&`) in non-virtual function parameters and use the `XM_CALLCONV` calling convention. This keeps vectors in SIMD registers and avoids unnecessary load/store round-trips.
  ```cpp
  // Correct — vector stays in registers
  [[nodiscard]] float XM_CALLCONV Distance(FXMVECTOR _a, FXMVECTOR _b);

  // Incorrect — forces store-to-memory then reload
  [[nodiscard]] float Distance(const XMFLOAT3& _a, const XMFLOAT3& _b);
  ```
- **Storage types** (`XMFLOAT3`, `XMFLOAT4X4`) are for struct members, serialization, and long-lived state. Load into `XMVECTOR` / `XMMATRIX` for computation; store the result back.
- Follow the DirectXMath parameter-position rules for `FXMVECTOR` / `GXMVECTOR` / `HXMVECTOR` / `CXMVECTOR` ordering (see [DirectXMath calling conventions](https://learn.microsoft.com/en-us/windows/win32/dxmath/pg-xnamath-internals#calling-conventions)).
- All new math utility functions go in `Neuron::Math` (`NeuronCore/GameMath.h`) — do not add math methods to storage types.

## Additional Notes

- Avoid adding includes already covered by `pch.h`.
- NeuronCore favors modern C++20/23 patterns; NeuronClient contains legacy code patterns (raw pointers, C-style strings).
- GameLogic (planned) is new server-side code — use modern C++ like NeuronCore.
- Build after changes to confirm compilation succeeds.
- See [MMO.md](../MMO.md) §9 for the full conventions reference.
