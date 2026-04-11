# GitHub Copilot Instructions

## Priority Guidelines

When generating code for this repository:

1. **Version Compatibility**: Always detect and respect the exact versions of languages, frameworks, and libraries used in this project.
2. **Context Files**: Prioritize patterns and standards defined in the .github/copilot directory if it exists.
3. **Codebase Patterns**: When context files don't provide specific guidance, scan the codebase for established patterns.
4. **Architectural Consistency**: Maintain the existing multi-project modular architecture and dependency boundaries.
5. **Code Quality**: Prioritize performance in all generated code.

## Technology Version Detection

Before generating code, scan the codebase to identify:

1. **Language Versions**: Detect the exact versions of programming languages in use.
	- Examine `CMakeLists.txt` files for `CMAKE_CXX_STANDARD` settings.
	- Check `CMakePresets.json` for build configuration details.
	- Never use language features beyond the detected version.

2. **Framework Versions**: Identify the exact versions of all frameworks.
	- Check `vcpkg.json` for dependency versions and features.
	- Respect version constraints when generating code.
	- Never suggest features not available in the detected framework versions.

3. **Library Versions**: Note the exact versions of key libraries and dependencies.
	- Generate code compatible with these specific versions.
	- Never use APIs or features not available in the detected versions.

## Context Files

Prioritize the following files in .github/copilot directory (if they exist):

- **architecture.md**: System architecture guidelines
- **tech-stack.md**: Technology versions and framework details
- **coding-standards.md**: Code style and formatting standards
- **folder-structure.md**: Project organization guidelines
- **exemplars.md**: Exemplary code patterns to follow

If the .github/copilot directory is not present, use guidance from `.github/coding-standards.md` and the existing codebase.

## Codebase Scanning Instructions

When context files don't provide specific guidance:

1. Identify similar files to the one being modified or created.
2. Analyze patterns for:
	- Naming conventions
	- Code organization
	- Error handling
	- Logging approaches
	- Documentation style
	- Testing patterns
3. Follow the most consistent patterns found in the codebase.
4. When conflicting patterns exist, prioritize patterns in newer files or files with higher test coverage.
5. Never introduce patterns not found in the existing codebase.

## Code Quality Standards

- Follow existing patterns for memory and resource management.
- Match existing patterns for handling computationally expensive operations.
- Follow established patterns for asynchronous operations.
- Apply caching consistently with existing patterns.
- Optimize according to patterns evident in the codebase.
- Use `winrt::com_ptr` instead of `Microsoft::WRL::ComPtr` for COM smart pointers throughout the codebase.
  - Use `.get()` instead of `.Get()` to obtain the raw pointer.
  - Use `= nullptr` instead of `.Reset()` to release a COM object.
  - Use `IID_GRAPHICS_PPV_ARGS(ptr)` (defined in `DirectXHelper.h`) instead of `IID_PPV_ARGS(&ptr)` when the target is a `com_ptr`.
  - `IID_PPV_ARGS` is only correct with raw pointers (`T**`) or `.put()` / `.Put()` return values.
- **DirectXMath vector passing**: Always pass 3D vectors as function parameters using `FXMVECTOR` (not `const GameVector3&` or `const XMFLOAT3&`) with the `XM_CALLCONV` calling convention to keep values in SIMD registers. Use `GameVector3`/`GameMatrix` only for storage (struct members, serialization). Follow DirectXMath parameter-position rules for `FXMVECTOR`/`GXMVECTOR`/`HXMVECTOR`/`CXMVECTOR`. All new math functions go in `Neuron::Math` (`NeuronCore/GameMath.h`).

## Documentation Requirements

- Follow the exact documentation format found in the codebase.
- Match the comment style and completeness of existing comments.
- Document parameters, returns, and exceptions in the same style where comments are used.
- Follow existing patterns for usage examples.
- Match class-level documentation style and content.

## Testing Approach

### Unit Testing

- Use the Visual Studio Native Unit Test Framework for any new unit tests.

### Integration Testing

- No integration-test framework is present in the current codebase. Do not invent new test frameworks unless explicitly requested.

## General Best Practices

- Follow naming conventions exactly as they appear in existing code.
- Match code organization patterns from similar files.
- Apply error handling consistent with existing patterns.
- Follow the same approach to testing as seen in the codebase.
- Match logging patterns from existing code.
- Use the same approach to configuration as seen in the codebase.

## Project-Specific Guidance

- Respect the solution boundaries and dependency graph:
  - Earthrise depends on NeuronClient, NeuronCore, GameRender.
  - NeuronClient depends on NeuronCore.
  - GameRender depends on NeuronClient, NeuronCore.
  - EarthRiseServer depends on NeuronServer, NeuronCore, GameLogic.
  - NeuronServer depends on NeuronCore.
  - GameLogic depends on NeuronCore (standalone simulation; server-only, not linked by client).
- Use `ASSERT` and `DEBUG_ASSERT` for assertions.
- Use `Neuron::DebugTrace` for debug logging and `Neuron::Fatal` for fatal errors.
- Files are named `PascalCase.cpp` / `PascalCase.h` and identifiers use `PascalCase` for classes and functions, `m_` for members, `g_` for global pointers, `UPPER_SNAKE_CASE` for constants.
- Do not add `#include` directives already covered by `pch.h`. All projects use `pch.h` / `pch.cpp`.
- Legacy code (NeuronClient) uses raw pointers and C-style strings; keep those patterns when editing those areas.
- GameLogic is new server-side code; use modern C++ patterns (like NeuronCore).
- NeuronCore favors modern C++ (e.g., `std::string_view`, `std::format`, `constexpr`, `[[nodiscard]]`, `noexcept`).
- Rendering: `ImRenderer` is legacy and should not be extended. New textured-quad rendering should target `SpriteBatch`.
- Game object renderer companions (per-category renderers like `ShipRenderer`, `AsteroidRenderer`, `StationRenderer`, and supporting types like `SpaceObjectRenderer`, `MeshCache`, `Camera`) should be placed in the GameRender project, not NeuronClient.
- Shared entity data types (`SpaceObjectCategory`, `EntityHandle`, `SpaceObject` struct, per-category data structs) belong in `NeuronCore/GameTypes/`.
- Simulation systems (movement, collision, combat, AI, spawning) belong in `GameLogic` (server-only).
- Build and verify after changes using CMake (`cmake --build --preset x64-debug` and `cmake --build --preset x64-release`).

## Data Serialization

- **Network packets**: Use `DataReader`/`DataWriter` — fixed 1400-byte buffer (`DATALOAD_SIZE`). Do not use for file I/O.
- **Disk I/O**: Use `BinaryFile::ReadFile`/`WriteFile` for raw binary I/O. Use `BinaryDataReader`/`BinaryDataWriter` (wrapping `byte_buffer_t` with cursor-based `Read<T>`/`Write<T>` via CRTP `SerializationBase`) for structured deserialization of zone definitions, object definitions, and tuning data. No size limit.
- Enum iteration: use `ENUM_HELPER` from `NeuronHelper.h` for sequential enums; use `ENUM_FLAGS_HELPER` for bitwise flag enums.

## Rendering Conventions

- **Depth buffer**: Reverse-Z — near plane maps to large Z, far plane to 0; use `D3D12_COMPARISON_FUNC_GREATER_EQUAL`; clear depth to `0.0f`. Prevents z-fighting at the 20 km draw distance.
- **Rendering coordinates**: Origin-rebased — subtract camera world position from all entity positions before GPU submission. Prevents `float` precision loss at zone edges (100 km zone).
- **No textures**: The flat-color pipeline assigns colors at runtime per-object/per-material. CMO meshes provide geometry only.
- **Mesh assets**: `Assets/Mesh/<CategoryFolder>/<MeshName>.cmo` — folder per category (e.g., `Hulls/`, `Asteroids/`, `Stations/`); `Hulls/` maps to `Ship` category. Mesh name (without `.cmo`) is the key in `MeshCache` and object definitions.
- **UI rendering**: `RenderCanvas()` composites HUD/UI on top of the 3D scene (same command list, same render target, no intermediate clear between `RenderScene()` and `RenderCanvas()`). UI uses `SpriteBatch` for textured quads and flat-color geometry for Darwinia-style chrome.

## Performance Constraints

- **`EventManager`**: Uses `std::scoped_lock` on every call — do not use for hot-path simulation systems (movement, collision, combat). Use direct function calls or per-system callbacks instead. Reserve `EventManager` for infrequent events only (player login, chat, docking, session management).
- **`ASyncLoader::WaitForLoad()`**: Busy-spins with `std::this_thread::yield()` — never call on the main thread. Use `co_await` in coroutine contexts (e.g., `GameApp::Startup()`) instead.
- **Coroutine startup**: `GameApp::Startup()` returns `Windows::Foundation::IAsyncAction`. Use `co_await` for async asset loading and leverage the `ASyncLoader` base class for loading-state tracking.

## Server Conventions

- **Server headers**: Server projects include `NeuronCoreBase.h` (pure C++ STL + Win32 + Winsock — no WinRT projections). Client projects include full `NeuronCore.h` (adds WinRT projections).
- **GameLogic is server-only**: Full simulation code (movement, collision, combat, AI) lives here. Client never links GameLogic.
- **Shared types in NeuronCore/GameTypes**: Entity data types needed by both client (for deserialization/rendering) and server (for simulation) are defined here.
