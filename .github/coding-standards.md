# Coding Standards

These conventions reflect the established patterns in the Starstrike codebase. Follow existing file-local style when a file deviates.

## Language and Toolchain

- Target platform: Windows 10+ (x64), MSIX packaging.
- Toolset: MSVC v145 (v143 fallback).
- Standard: `stdcpplatest` (Debug), `stdcpp20` (Release).
- Precompiled headers: every project uses `pch.h` / `pch.cpp`.

## Naming

- Files: `PascalCase.cpp` / `PascalCase.h`.
- Classes/structs: `PascalCase`.
- Methods/functions: `PascalCase`.
- Member variables: `m_` prefix with `camelCase` (e.g., `m_homeDir`).
- Global pointers: `g_` prefix (e.g., `g_app`).
- Constants/macros: `UPPER_SNAKE_CASE`.

## Formatting and Layout

- Indentation: 2 spaces in app-level code; some library code uses tabs. Match the file.
- Braces: control-flow opening brace on the same line; function/class definitions vary by file. Match surrounding style.
- Include guards: use `#pragma once` (not `#ifndef` guards).

## Includes and Headers

- Do not add includes already covered by `pch.h`.
- When adding new source files, configure PCH usage (`Use` for `.cpp`, `Create` for `pch.cpp`).
- New `.cpp` files must include `pch.h` as the first line.

## Assertions and Diagnostics

- Use `ASSERT_TEXT(condition, msg)` and `DEBUG_ASSERT(x)` (avoid standard `assert`).
- Debug logging: `Neuron::DebugTrace(fmt, args...)` with `std::format` syntax.
- Fatal errors: `Neuron::Fatal(fmt, args...)`.

## Pointers and Ownership

- Legacy code uses raw pointers and manual `new` / `delete`. Refactor to smart pointers unless requested.
- New code in NeuronCore and Starstrike should use `std::unique_ptr` or `std::shared_ptr`.

## COM Smart Pointers

Use `winrt::com_ptr<T>` throughout the codebase (not `Microsoft::WRL::ComPtr<T>`):
- `.get()` to obtain the raw pointer (not `.Get()`).
- `= nullptr` to release (not `.Reset()`).
- `IID_GRAPHICS_PPV_ARGS(ptr)` when the target is a `com_ptr` (not `IID_PPV_ARGS(&ptr)`).

## NeuronCore Modern C++ Usage

When working inside NeuronCore:

- Prefer `std::string_view`, `std::format`, `constexpr`, `[[nodiscard]]`, `noexcept`.
- Prefer `std::vector`, `std::array`, `std::ranges`, `std::mdspan` where appropriate.
- Use C++/WinRT for Windows Runtime API access.

## Legacy Code (NeuronClient, GameLogic)

- Keep C-style strings (`char*`) consistent with surrounding code.
- Use `strdup`, `new[]` / `delete[]` as the file already does.
- Do not introduce new uses of unsafe C string functions (`sprintf`, `strcpy`, `strcat`). Prefer `snprintf`, `std::string`, or `std::format` in new code.

## Build Verification

- Run a build in both `Debug x64` and `Release x64` after every change.
- Zero new compiler warnings should be introduced.

## Documentation Structure

The project follows this standard documentation layout:

```
Starstrike/
├── README.md                        # Project overview and quick start
├── docs/
│   ├── architecture.md              # Module structure, subsystem design, dependency rules
│   ├── development-guide.md         # Setup, build, debugging, workflow
│   └── contributing.md              # PR process, commit messages, review standards
├── .github/
│   ├── agents.md                    # AI agent instructions and project summary
│   ├── coding-standards.md          # This file
│   ├── copilot-instructions.md      # Copilot/AI assistant guidance
│   └── agents/                      # Specialised AI agent definitions
│       ├── Architect.agent.md
│       ├── planner.agent.md
│       ├── SoftwareEngineer.agent.md
│       └── RefactorCleaner.agent.md
├── CI.md                            # Codebase improvement roadmap
├── MathPlan.md                      # Math type migration plan
└── MatrixConv.md                    # Matrix convention plan
```

### Documentation Conventions

- All documentation is written in Markdown (`.md`).
- Use ATX-style headings (`#`, `##`, `###`).
- Use fenced code blocks with a language identifier (` ```cpp `, ` ```powershell `, etc.).
- Use tables for reference information (file lists, option comparisons, issue summaries).
- Keep documentation accurate: update docs whenever the corresponding code or process changes.
- Inline code comments should explain *why*, not *what*, unless the *what* is genuinely non-obvious.

### When to Update Documentation

| Change | Documentation to update |
|---|---|
| New subsystem or module | `docs/architecture.md`, `README.md` |
| New development prerequisite or build step | `docs/development-guide.md` |
| New PR or review process | `docs/contributing.md` |
| New coding convention | `.github/coding-standards.md` |
| New or changed AI agent behaviour | `.github/agents.md` or `.github/agents/*.agent.md` |
| New improvement phase | `CI.md` |
