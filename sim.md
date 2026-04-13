# Earthrise DirectX 12 Renderer — Simplification & Design Review

**Scope:** `GameRender/` and rendering-adjacent code in `Earthrise/` (notably `GameApp.cpp`).
**Goal:** Identify design flaws, reduce boilerplate, and propose concrete simplifications.
**Author perspective:** DirectX 12 engine review.

---

## TL;DR

The renderer is a clean, single-threaded D3D12 implementation with a sound ring-buffered upload heap and shader-visible descriptor heap. It is correct, but it has a major **scalability cliff** (per-particle draw calls — up to 4 096 per frame at pool capacity), a **blocking static upload path** that stalls the render thread on first-use mesh generation, and **PSO construction boilerplate** spread across 8 files.

Shaders are already compiled offline via DXC (SM 6.7), `FrameConstants` is already unified in `FlatColorPipeline.h`, and root signatures fall into three distinct patterns (not one). The remaining work is about **particle batching, async uploads, a PSO builder, and hardening** (descriptor budget, device-removed hooks).

Addressing the items in **§2 Critical** and **§3 High** would remove the two bottlenecks most likely to bite during gameplay scale-up and reduce PSO boilerplate by an estimated 500–900 lines.

---

## 1. Architecture Snapshot

| Concern | Current design | Verdict |
|---|---|---|
| Command queue / list | Single DIRECT queue, single command list per frame, owned by `Core` | ✅ simple, correct |
| Frame pacing | 3-frame back-buffer ring; `BeginFrame(frameIndex)` resets upload & SRV heaps | ✅ correct ring pattern |
| Upload heap | `UploadHeap` — 4 MB × 3 frames, persistently mapped, 256-byte aligned | ✅ correct, fixed size |
| CBV binding | Root CBV (GPU VA), no descriptors | ✅ optimal |
| Shader-visible heap | 256 descriptors × 3 frames, ring-allocated | ⚠ fragile (fatal on overflow) |
| Static uploads | `GpuResourceManager` — dedicated allocator/list/fence, **blocking wait** | ⚠ startup-only safe |
| PSO strategy | One PSO per renderer, offline-compiled HLSL (DXC SM 6.7) | ⚠ root-sig/PSO duplication |
| Renderers | 9 independent classes, each owning a pipeline | ⚠ PSO boilerplate (root sigs are intentionally distinct) |
| Multi-threading | None | Acceptable today |
| Device loss | Not handled in GameRender | ⚠ latent risk |

---

## 2. Critical Issues (fix before any scale test)

### 2.1 ParticleRenderer issues one draw call *per particle*

**File:** `GameRender/ParticleRenderer.cpp:32-56`

```cpp
for (const auto& p : particles)
{
  ...
  m_pipeline.SetDrawConstants(_cmdList, _cbAlloc, dc);   // 256-byte CB allocation
  _cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
  _cmdList->DrawInstanced(1, 1, 0, 0);                    // 1 vertex, 1 instance
}
```

Each particle incurs: 1 `Allocate` from the upload ring, 1 `SetGraphicsRootConstantBufferView`, 1 `IASetPrimitiveTopology` (redundantly set every iteration), and 1 `DrawInstanced(1,…)`. The pool cap is `MAX_PARTICLES = 4096`. At capacity that's ~16 000 command-list calls and ~1 MB of CB waste per frame. Topology state is also redundantly set inside the hot loop.

**Sizing note:** At 32 bytes/particle the instanced structured buffer would be ~128 KB — well within the 4 MB per-frame upload heap.

**Recommendation — pick one:**

1. **Instanced quad** (simplest win): upload per-particle data (pos/size/color) into one structured buffer allocated from the upload ring, bind as SRV, call `DrawInstanced(6, N, 0, 0)`. One draw call, ~32 bytes/particle instead of 256. Requires: a new `ParticleVS.hlsl` that reads from `StructuredBuffer<ParticleData>` via `SV_InstanceID`, a new root signature with an SRV slot (current `FlatColorPipeline` has no SRV binding), and a dedicated PSO. The current setup reuses `FlatColorPipeline` with a Position+Normal input layout — this must be replaced entirely.
2. **GPU particle system**: compute-shader simulation + `ExecuteIndirect`. Better long-term, larger refactor.

Either approach removes the `IASetPrimitiveTopology` from the loop and reduces CB traffic ~32× at pool capacity.

### 2.2 Blocking static-asset uploads

**File:** `GameRender/GpuResourceManager.cpp:90-97` (and `192-197`)

```cpp
++s_copyFenceValue;
check_hresult(Core::GetCommandQueue()->Signal(s_copyFence.get(), s_copyFenceValue));
if (s_copyFence->GetCompletedValue() < s_copyFenceValue) {
  check_hresult(s_copyFence->SetEventOnCompletion(s_copyFenceValue, s_copyFenceEvent.get()));
  WaitForSingleObjectEx(s_copyFenceEvent.get(), INFINITE, FALSE);   // full CPU stall
}
```

Every `CreateStaticBuffer` / `CreateStaticTexture` call blocks the CPU until the GPU copy is done, and it **reuses the main direct queue** (not a copy queue). At startup (mesh preload, fonts) this is fine; but `SurfaceRenderer::GetSurfaceMesh` lazily builds new meshes on first reference, and this path reaches `CreateStaticBuffer` mid-frame — every new (mesh, surface-type) combination is a full pipeline stall.

**Recommendation:**

- Add an **async path**: batch pending uploads each frame, issue them on a `COPY` queue, track completion with a fence value, and defer resource use until the fence has passed (or keep a “pending” list and stitch barriers onto the main list at frame start).
- Keep a blocking `CreateStaticBufferSync` for truly one-shot startup paths, but make it an opt-in, not the default.
- **Option C — Pre-load after zone load**: Surface types are a fixed enum of 5, but meshes are discovered during zone load (CMO files loaded on-demand by `CmoLoader`). Once a zone is loaded and its asteroid mesh set is known, eagerly build all `5 × N_meshes` surface meshes during the zone-load coroutine. Only `AsteroidRenderer` calls `GetSurfaceMesh` — no other category needs surface coloring. Simpler than a full async upload path if the mesh set is small.
- Fix the latent bug: `s_copyAllocator` is reset on every call (line 73) with no check that a prior upload is still in flight; today it's safe only because the call is blocking. An async version must allocate per-submission or round-robin.

### 2.3 Shader-visible descriptor heap is fatal on overflow

**File:** `GameRender/ShaderVisibleHeap.cpp:45-46`

```cpp
if (m_currentIndex + count > m_descriptorsPerFrame)
  Neuron::Fatal("ShaderVisibleHeap: frame region exhausted");
```

256 descriptors per frame is enough today. Measured per-frame SRV allocations: bloom ~6, SunBillboard 1, BitmapFont 1 per `BeginDraw` — peak is likely ~15–20, giving ~10× headroom. However, the `PostProcess::makeSRV` lambda recreates SRVs for the same static textures every frame (see §3.1), and any future UI growth (chat log, inventory, tooltips) will push the watermark higher with no warning before a hard crash.

**Recommendation:**

- **Instrument first**: add high-watermark logging per frame in debug builds via `DebugTrace`. This is the highest-value change — it makes the budget visible.
- Raise budget from 256 to 1 024 (trivial change, 4× headroom). This is sufficient for years given current allocation rates.
- Replace `Neuron::Fatal` with graceful degradation: skip the draw and warn. A spill heap requires a new `SetDescriptorHeaps` call which invalidates prior bindings — prefer the larger budget.

### 2.4 PSO construction boilerplate

**Files:** `FlatColorPipeline.cpp`, `VertexColorPipeline.cpp`, `Starfield.cpp`, `SunBillboard.cpp`, `TacticalGrid.cpp`, `SpriteBatch.cpp`, `BitmapFont.cpp`, `PostProcess.cpp`.

Shaders are already compiled offline via DXC to `CompiledShaders/*.h` headers (SM 6.7). The legacy `PipelineHelpers::CompileShader()` runtime path is dead code (see §4.8).

Every renderer hand-rolls a `D3D12_GRAPHICS_PIPELINE_STATE_DESC` with 95% identical fields (reverse-Z, `GREATER_EQUAL`, back-buffer format, sample mask, etc.). This is the real source of boilerplate.

**Root signatures are NOT identical.** They fall into three distinct patterns:

| Pattern | Users | Layout |
|---|---|---|
| Dual CBV, no SRV | FlatColor, VertexColor | `{b0, b1}` |
| Single CBV, no SRV | Starfield, TacticalGrid, SpriteBatch | `{b0}` |
| CBV + SRV table + sampler | SunBillboard, BitmapFont, PostProcess | `{b0, table(t0..tn), sampler}` |

A single "universal" root signature would force all renderers to bind an SRV descriptor table they don't use. On most hardware this is free, but it adds conceptual complexity. **Recommendation: leave root signatures per-renderer** (or introduce 2-3 shared variants if duplication bothers you — the savings are ~20 lines per renderer, ~120 total).

**PSO builder** is the higher-value target. A `PsoBuilder` struct with fluent setters (`.WithVS(...).WithPS(...).WithInputLayout(...).WithBlend(Additive).WithDepth(ReverseZ)`) replaces every renderer's `CreatePipelineState` boilerplate.

Estimated reduction: ~500-900 lines across the 8 pipeline files (PSO construction only, not root sigs).

---

## 3. High-priority Issues

### 3.1 Descriptors recreated every frame for static textures

**Files:**
- `GameRender/PostProcess.cpp:267-277` — `makeSRV` lambda inside the render function.
- `GameRender/SunBillboard.cpp:189-193` — copies `m_glowSRV_CPU` into the shader-visible heap each call.
- `GameRender/BitmapFont.cpp` — same pattern for the glyph atlas.

All three describe **static** resources: the back-buffer-format bloom SRVs alias the same textures every frame, and the sun glow / font atlas never change. Yet each frame we burn descriptor slots and either call `CreateShaderResourceView` or `CopyDescriptorsSimple` just to re-publish them.

**Note:** Bloom RTs are recreated on resize (`PostProcess::OnResize` → `CreateResources`), so their SRV descriptors become invalid after resize. Only the sun glow texture and font atlas are truly static across the session.

**Recommendation:** Introduce a small **"persistent" segment** in the shader-visible heap (first N slots, set once at init, never reset by `BeginFrame`). Store GPU handles for the glow texture and font atlases. Bloom RT SRVs should be recreated only on resize (cache in `PostProcess` members, refresh in `OnResize`) rather than every frame — this still eliminates per-frame churn without requiring true persistence. The per-frame ring starts after the persistent segment.

### 3.2 Resource barriers are not batched

**File:** `GameRender/PostProcess.cpp:281-287, 313-324, 338-346, ...`

```cpp
auto barrier  = CD3DX12_RESOURCE_BARRIER::Transition(sceneRT, RT, SRV);
cmdList->ResourceBarrier(1, &barrier);
auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(m_brightPassRT.get(), SRV, RT);
cmdList->ResourceBarrier(1, &barrier2);
```

Each bloom function issues 8 single-barrier calls across 4 pass boundaries. Most barriers are isolated (one resource transition per pass edge), but lines 198+206 (bright-pass exit + H-blur entry) and lines 223+230 (H-blur exit + V-blur entry) are adjacent pairs that transition different resources at the same boundary — these can be batched into a single `ResourceBarrier(2, arr)` call.

**Recommendation:** Batch adjacent barriers at each pass boundary into one `ResourceBarrier(N, arr)` call. A helper (`BarrierBatch` with `.Add(...)` / `.Flush(cmdList)`) makes this ergonomic. Practical savings are modest (8 calls → ~5 per bloom function), but this is easy to do and prevents the pattern from worsening as passes are added.

### 3.3 Root Signature version 1.0

**File:** `GameRender/PipelineHelpers.cpp:47` — `D3D_ROOT_SIGNATURE_VERSION_1`.

1.1 unlocks static-data and volatile-descriptor flags that let the driver optimize CBV/SRV access. It’s a one-line change with runtime feature-level detection and negligible risk.

**Recommendation:** Use `D3D12SerializeVersionedRootSignature` and prefer `VERSION_1_1` with a fallback to 1.0 if `CheckFeatureSupport(ROOT_SIGNATURE)` reports otherwise.

### 3.4 No device-removed handling in GameRender

`NeuronClient/Graphics/GraphicsCore.cpp` **does** check `DXGI_ERROR_DEVICE_REMOVED` after `Present()` and calls `HandleDeviceLost()` → `OnDeviceRestored()` via `IDeviceNotify`. However, **GameRender renderers have no recovery hooks**: static/cached GPU resources (PSOs, descriptor heaps, mesh buffers) become invalid after a TDR but no `RecreateGpuResources()` callback exists.

**Recommendation:** Have each GameRender renderer (or a central `RenderPipelineCache`) implement an `OnDeviceLost()` / `OnDeviceRestored()` pair, registered with `Core` via `IDeviceNotify`. On restore, rebuild PSOs, root signatures, and re-upload static buffers. At minimum, log `GetDeviceRemovedReason` and exit cleanly rather than continuing with stale handles.

### 3.5 `SurfaceRenderer::GetSurfaceMesh` CPU work on first use

`GetSurfaceMesh()` generates procedural UV spheres synchronously on the render thread the first time a unique (mesh, surface-type) pair is seen, then immediately calls the blocking `CreateStaticBuffer`. Combined with §2.2 this is a noticeable hitch. Only `AsteroidRenderer` calls `GetSurfaceMesh` (via `CategoryRenderers.h`) — no other entity category uses surface coloring.

Surface types are a fixed enum of 5 (`SurfaceType::Default` through `SurfaceType::Earth`), but meshes are loaded on-demand from CMO files during zone load, so the full set of combinations isn't known until zone data is parsed.

**Recommendation:** After zone load completes and the asteroid mesh set is known, eagerly build all `5 × N_meshes` surface meshes during the zone-load coroutine (§2.2 Option C). If zones have many unique asteroid meshes, use the async upload path instead. Cache key the same way.

---

## 4. Medium-priority Simplifications

### 4.1 ~~Collapse `SpaceObjectRenderer` and `SurfaceRenderer`~~ NOT RECOMMENDED

Investigation shows these renderers differ more than initially assessed:
- Different pipelines (`FlatColorPipeline` vs `VertexColorPipeline`)
- Different color models (runtime color parameter vs. colors baked at mesh load via `BuildColorArray`)
- Different mesh types (`Mesh` vs `SurfaceMesh`)
- Combined they're only ~130 lines

The template/interface overhead to merge them would likely *add* complexity. **Drop this item.**

### 4.2 SpriteBatch / BitmapFont — extract shared `FlushBatch` only

Both own a CPU vertex buffer, batch quads, allocate from the upload heap, and draw in `FlushBatch`. The `FlushBatch()` methods are ~15 lines each of identical code. However, the classes differ meaningfully in pipeline (solid color vs textured + different samplers), vertex format (`SpriteVertex` vs `FontVertex`), and root signature pattern.

**Recommendation:** Extract a `FlushBatch<TVertex>` helper template (~30 lines saved) rather than merging the classes. The pipelines and `Begin`/`End` logic should stay separate.

### 4.3 `s_pso` / `s_rootSignature` static-per-class pattern in BitmapFont

**File:** `GameRender/BitmapFont.cpp:70-72`

Class-scope statics (`s_pso`, `s_rootSignature`, `s_pipelineReady`) are shared across all `BitmapFont` instances. Two instances exist at runtime (`m_editorFont`, `m_speccyFont` in `GameApp`). The pattern is safe during normal operation (second instance skips pipeline creation via `if (s_pipelineReady) return`), but fragile under teardown/device-recreate — there is no `OnDeviceLost` that resets `s_pipelineReady`, so a TDR leaves both instances pointing at stale GPU objects.

**Recommendation:** Move to a `FontPipeline` singleton owned by the renderer or to the shared PSO registry from §2.4. At minimum, add a `ResetPipeline()` static that clears `s_pipelineReady` for use in device-lost recovery (§3.4).

### 4.4 `SetDescriptorHeaps` calls in PostProcess

`ApplyBloom` (line 142) and `ApplyBloomAdditive` (line 305) each call `SetDescriptorHeaps` at entry. These are in **separate functions**, so they are not redundant — each must set the heap in case the caller changed it between calls. However, within each function the heap is set once and never changed, which is correct. No action needed unless the two functions are combined into one code path.

### 4.5 Particle CB packing

If §2.1 is deferred: at least pack multiple particles per 256-byte CB (e.g. 4 per CB) to quarter the CB traffic without touching the draw model.

### 4.6 ~~Unify `FrameConstants` across renderers~~ ✅ DONE

`FrameConstants` is already defined once in `FlatColorPipeline.h` and shared by all renderers (`FlatColorPipeline`, `VertexColorPipeline`, `ParticleRenderer`, `SurfaceRenderer`). No action needed.

### 4.7 Debug names and `SetName` coverage

Some objects have debug names (`GpuResourceManager`, `PostProcess`) but most descriptor heaps, meshes in `MeshCache`, and per-renderer PSOs don’t. Names are free at runtime and invaluable in PIX / Nsight captures. Add a small `NAME_D3D(obj, L"...")` macro and sprinkle it.### 4.8 Delete dead `PipelineHelpers::CompileShader()`

**File:** `GameRender/PipelineHelpers.cpp`

`PipelineHelpers.cpp` includes `d3dcompiler.h` and defines a `CompileShader()` function that calls `D3DCompile()` at runtime. This function has **zero callers** — all shaders are compiled offline via DXC. The dead code adds an unnecessary link dependency on `d3dcompiler.lib` and is a maintenance trap (someone might accidentally call it instead of adding a shader to `CompileShaders.cmake`).

**Recommendation:** Delete `CompileShader()`, remove the `#include <d3dcompiler.h>` from `PipelineHelpers.cpp` (keep it only if other functions in the file need it), and remove `d3dcompiler.lib` from the GameRender link line if no other references remain.

---

## 5. Lower-priority / Cosmetic

- **Topology in render loop** (`ParticleRenderer.cpp:54`) — set once before the loop.
- **Upload-heap size** — 4 MB × 3 is arbitrary; expose via config, log high-watermark per frame in debug.
- **Mesh-cache eviction** — `MeshCache` is append-only; for long sessions with many asteroids consider an LRU policy.
- **RTV descriptor churn on resize** — `PostProcess::OnResize` re-allocates RTVs but never releases the old ones back to the CPU heap. Low impact today; add a free-list if resizing becomes common.
- **`RECT` in a rendering API** (`SpriteBatch::DrawRect`) — prefer a float rect to avoid implicit int-to-float conversions at call sites.
- ~~**Shader model 5.1 everywhere**~~ ✅ Already using SM 6.7 via DXC offline compilation.

---

## 5½. Baseline Metrics (measure before starting)

Before beginning work, capture these numbers to validate improvements:

| Metric | How to measure | Expected current value |
|---|---|---|
| Draw calls / frame (idle scene) | PIX single-frame capture | ~20-30 |
| Draw calls / frame (4 096 particles) | PIX | ~4 116-4 126 |
| Descriptor high-watermark / frame | Add `DebugTrace` in `ShaderVisibleHeap::Allocate` | ~15-20 (< 256) |
| Upload-heap utilization / frame | Log `m_currentOffset` at end of frame | Unknown (< 4 MB) |
| Frame time (idle) | `QueryPerformanceCounter` delta | Baseline |
| Frame time (4 096 particles) | Same | Baseline + particle overhead |
| `GpuResourceManager` fence waits / session | Count in `CreateStaticBuffer` | Unknown |
| Startup time (app launch → first frame) | Timer in `GameApp::Startup` | Baseline |

---

## 5¾. Testing & Validation Strategy

Each task requires different validation:

- **Visual regression**: Capture a reference screenshot before and after. Compare manually or with image diff tool. Required for: §2.1, §2.4, §3.1.
- **PIX capture**: Use a single-frame capture to verify draw call counts, barrier placement, descriptor usage. Required for: §2.1, §3.1, §3.2.
- **Unit tests** (`EarthRiseTests/`): `RenderMathTests.cpp` covers math. No GPU tests exist today. For CPU-side changes (particle buffer packing, barrier batch helper), add unit tests. Required for: §2.1 (buffer layout), §3.2 (BarrierBatch helper).
- **Stress test**: Spawn maximum particle count (4 096), verify no descriptor overflow or upload-heap exhaustion. Required for: §2.3, §2.1.
- **TDR simulation**: Set `HKLM\SYSTEM\CurrentControlSet\Control\GraphicsDrivers\TdrDelay` to a low value and force a GPU hang. Required for: §3.4.
- **Build verification**: `cmake --build --preset x64-debug` and `cmake --build --preset x64-release` after every change.

---

## 6. Recommended Order of Work

| Order | Task | Risk | Payoff | Status | Validation |
|---|---|---|---|---|---|
| 1 | §2.1 Particle instancing | Low | Largest perf win (4 096 draws -> 1) | TODO | PIX capture: verify 1 draw call; frame-time delta |
| 2 | §4.8 Delete dead `CompileShader` | Low | 5-min cleanup; removes `d3dcompiler.lib` | TODO | Build succeeds; `d3dcompiler.h` no longer included |
| 3 | §2.3 Descriptor budget + diagnostics | Low | Safety net + visibility | TODO | High-watermark log works; budget raised to 1 024 |
| 4 | §3.1 Persistent descriptor segment | Low | Removes per-frame SRV churn | TODO | Debug: descriptor high-watermark drops |
| 5 | §3.2 Batched barriers | Low | Micro-perf + readability | TODO | PIX: fewer barrier calls per frame |
| 6 | §2.2 Async static uploads + §3.5 | Medium | Removes mid-game hitches | TODO | Profile: no fence waits mid-frame |
| 7 | §2.4 PSO builder | Medium | ~500-900 lines removed | TODO | Full visual regression pass |
| 8 | §3.3 Root Sig 1.1 | Low | Minor perf | TODO | `CheckFeatureSupport` confirms 1.1; fallback tested |
| 9 | §3.4 Device-removed hooks | Medium | Robustness | TODO | Simulate TDR via `TdrDelay`; clean shutdown |
| -- | §4.6 Unify `FrameConstants` | -- | -- | DONE | Already unified in `FlatColorPipeline.h` |
| -- | Offline HLSL compilation | -- | -- | DONE | DXC SM 6.7 via `CompileShaders.cmake` |
| -- | §4.1 Collapse renderers | -- | -- | DROPPED | Negative ROI after investigation |

---

## 7. Estimated Impact

- **Lines of code removed:** ~600-1 000 in `GameRender/` (PSO builder boilerplate + dead `CompileShader`) out of ~3 500-4 500 total lines, i.e. ~15-25%. (§4.1 renderer merger dropped; root-sig sharing deferred.)
- **Draw-call reduction per frame at 4 096 particles:** ~4 096 -> 1 (three orders of magnitude).
- **Per-frame upload-heap use at 4 096 particles:** ~1 MB -> ~128 KB.
- **First-use hitches** from lazy surface meshes: eliminated for zones with few asteroid meshes (via Option C post-zone-load preload). For zones with many meshes, requires async upload path.
- **Shader build-time errors:** already caught at compile time (DXC offline compilation).

---

## 8. What’s Already Good (keep it)

- Root-CBV binding strategy — optimal for small per-draw data.
- Ring-buffered upload heap with persistent mapping — textbook pattern, correctly implemented.
- Reverse-Z depth with `GREATER_EQUAL` — modern best practice.
- Starfield as a single static VB + one draw call — exemplary.
- CMO mesh caching in `SurfaceRenderer` — right idea, just needs the async completion path.
- Clear separation between `Core` (device/queue/swap chain) and `GameRender` (passes).
- `FrameConstants` unified in `FlatColorPipeline.h` and shared by all renderers.
- Offline shader compilation via DXC (SM 6.7) with proper CMake integration.

The foundation is solid; the remaining work is about **particle batching** (biggest perf win), **a PSO builder** (biggest code reduction), **async uploads** (eliminates hitches), and **hardening** (descriptor budget, device-removed hooks). Root signature sharing is low-value given the three distinct patterns; renderer mergers are negative ROI.

---

## 9. Scope Notes

This review covers `GameRender/` rendering pipelines and `Earthrise/GameApp.cpp`. The following GameRender modules are **out of scope** but functioning correctly:

- **Camera.cpp** — origin-rebasing camera logic (correct, no perf concern)
- **CmoLoader.cpp / DdsLoader.cpp** — asset file parsers (called via `GpuResourceManager`, affected by §2.2)
- **LandscapeTexture.cpp** — gradient-based surface color sampling (used only by `SurfaceRenderer`)
- **MeshCache.cpp** — append-only mesh cache (eviction noted in §5)
- **UploadHeap.cpp / ShaderVisibleHeap.cpp** — allocators (covered by §2.3)
- **CategoryRenderers.h** — per-entity-category renderer wrappers (`AsteroidRenderer`, `ShipRenderer`, etc.); only `AsteroidRenderer` calls `SurfaceRenderer`
- **ParticleSystem.cpp** — CPU-side particle simulation (`MAX_PARTICLES = 4096`, lifetime 0.3–1.2s, burst + individual spawn)
