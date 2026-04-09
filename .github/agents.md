# AGENTS.md

## Project Overview

Starstrike is a Windows-native C++ real-time strategy game built with Visual Studio (.slnx). It targets Windows 10+ (x64) and ships as an MSIX app using Windows App SDK 1.8 and WinUI 3. Rendering is Direct3D 12. The solution contains multiple static libraries (NeuronClient, NeuronCore, NeuronServer, GameLogic) and the Starstrike app.

**Dependency graph:**
```
Starstrike ──► NeuronClient ──► NeuronCore
          ──► GameLogic
          ──► NeuronCore
NeuronServer ──► NeuronCore
```

## Documentation

| Document | Purpose |
|---|---|
| [README.md](../README.md) | Project overview and quick start |
| [docs/architecture.md](../docs/architecture.md) | Module structure, dependency rules, subsystem design |
| [docs/development-guide.md](../docs/development-guide.md) | Setup, build, debugging, and development workflow |
| [docs/contributing.md](../docs/contributing.md) | PR process, commit messages, review standards |
| [coding-standards.md](coding-standards.md) | Naming, formatting, language conventions |
| [CI.md](../CI.md) | Phased codebase improvement roadmap |
| [MathMigration.md](../MathMigration.md) | Math type migration plan (LegacyVector3/Matrix34 → DirectXMath) |

## Setup Commands

- Install dependencies: `nuget restore Starstrike.slnx`
- Start development server: N/A (desktop app)
- Build for production: `msbuild Starstrike.slnx /p:Configuration=Release /p:Platform=x64`

## Development Workflow

- Open `Starstrike.slnx` in Visual Studio 2022 (v17.x+), select `x64` + `Debug`, and build/run the `Starstrike` project.
- No hot reload/watch mode is configured.
- No required environment variables are defined.
- See [docs/development-guide.md](../docs/development-guide.md) for full setup and debugging instructions.

## Testing Instructions

- Run all tests: N/A (no automated test suite configured)
- Run unit tests: N/A
- Run integration tests: N/A
- Test coverage: N/A
- Manual validation is expected after rendering or gameplay changes.
- New utilities in NeuronCore may be covered by Visual Studio Native Unit Tests.

## Code Style

- Naming: files `PascalCase.cpp/.h`, classes `PascalCase`, methods `PascalCase`, members `m_` + `camelCase`, globals `g_` prefix, constants `UPPER_SNAKE_CASE`.
- Indentation: 2 spaces in app-level code; some libraries use tabs. Match the file.
- Include guards: use `#pragma once` (not `#ifndef`).
- Assertions/logging: `ASSERT_TEXT`, `DEBUG_ASSERT`, `Neuron::DebugTrace`, `Neuron::Fatal`.
- PCH: all projects use `pch.h` / `pch.cpp`. New `.cpp` files must include `pch.h` first.
- Rendering: do not add new functionality to `ImRenderer`; use `SpriteBatch` for new textured-quad work.
- COM pointers: use `winrt::com_ptr<T>`, `.get()`, and `= nullptr` (not `Microsoft::WRL::ComPtr`, `.Get()`, `.Reset()`).
- Full standards in [coding-standards.md](coding-standards.md).

## Build and Deployment

- Build the `Starstrike` project for MSIX packaging (x64).
- Use `Debug` for local dev and `Release` for shipping builds.
- Deployment is via MSIX produced by the Starstrike app project.
- Both `Debug x64` and `Release x64` must build cleanly before merging.

## Pull Request Guidelines

- Title format: `[component] Brief description`
- Required checks: build `x64` in `Debug` and `Release`.
- Include a short summary of gameplay/graphics impact when applicable.
- Full guidelines in [docs/contributing.md](../docs/contributing.md).

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
- **Storage types** (`XMFLOAT3`, `XMFLOAT4X4`) are for struct members, serialization, and long-lived state. Load into `XMVECTOR` / `XMMATRIX` for computation; store the result back. Transitional types (`GameVector3`, `GameMatrix`, `Transform3D`) are being eliminated — see [MathMigration.md](../MathMigration.md).
- Follow the DirectXMath parameter-position rules for `FXMVECTOR` / `GXMVECTOR` / `HXMVECTOR` / `CXMVECTOR` ordering (see [DirectXMath calling conventions](https://learn.microsoft.com/en-us/windows/win32/dxmath/pg-xnamath-internals#calling-conventions)).
- All new math utility functions go in `Neuron::Math` (`NeuronCore/GameMath.h`) — do not add math methods to storage types.
- See [MathMigration.md](../MathMigration.md) for the full migration roadmap from legacy types.

## Additional Notes

- Avoid adding includes already covered by `pch.h`.
- NeuronCore favors modern C++20/23 patterns; NeuronClient/GameLogic contain legacy code patterns (raw pointers, C-style strings).
- Build after changes to confirm compilation succeeds.
- See [CI.md](../CI.md) for the phased plan to modernise unsafe and legacy patterns.
