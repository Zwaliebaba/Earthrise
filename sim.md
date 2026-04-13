# Earthrise DirectX 12 Renderer — Simplification & Design Review

**Scope:** `GameRender/` and rendering-adjacent code in `Earthrise/` (notably `GameApp.cpp`).
**Goal:** Identify design flaws, reduce boilerplate, and propose concrete simplifications.
**Author perspective:** DirectX 12 engine review.

---

## TL;DR

The renderer is a clean, single-threaded D3D12 implementation with a sound ring-buffered upload heap and shader-visible descriptor heap. It is correct, but it has accumulated a lot of **structural duplication** (9 near-identical PSO/root-signature setups, HLSL embedded in every .cpp, hand-rolled one-shot renderers) and a handful of **scalability cliffs** (per-particle draw calls, blocking static uploads, fixed ring-buffer sizes, no descriptor overflow recovery).

Addressing the items in **§2 Critical** and **§3 High** would shrink the rendering codebase by an estimated 25–35% while removing the two bottlenecks most likely to bite during gameplay scale-up (particles and asset streaming).

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
| PSO strategy | One PSO per renderer, HLSL embedded as C++ string literals | ❌ lots of duplication |
| Renderers | 9 independent classes, each owning a pipeline | ❌ boilerplate sprawl |
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

Each particle incurs: 1 `Allocate` from the upload ring, 1 `SetGraphicsRootConstantBufferView`, 1 `IASetPrimitiveTopology` (redundantly set every iteration), and 1 `DrawInstanced(1,…)`. For 2 000 particles that's ~8 000 command-list calls and ~512 KB of CB waste. Topology state is also set in the hot loop when it could be set once before the loop.

**Recommendation — pick one:**

1. **Instanced quad** (simplest win): upload per-particle data (pos/size/color) into one structured buffer allocated from the upload ring, bind as SRV, call `DrawInstanced(6, N, 0, 0)`. One draw call, ~32 bytes/particle instead of 256.
2. **GPU particle system**: compute-shader simulation + `ExecuteIndirect`. Better long-term, larger refactor.

Either approach removes the `IASetPrimitiveTopology` from the loop and reduces CB traffic ~8×.

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
- Fix the latent bug: `s_copyAllocator` is reset on every call (`:73`) with no check that a prior upload is still in flight; today it’s safe only because the call is blocking. An async version must allocate per-submission or round-robin.

### 2.3 Shader-visible descriptor heap is fatal on overflow

**File:** `GameRender/ShaderVisibleHeap.cpp:45-46`

```cpp
if (m_currentIndex + count > m_descriptorsPerFrame)
  Neuron::Fatal("ShaderVisibleHeap: frame region exhausted");
```

256 descriptors per frame is enough today, but: bloom allocates ~4 SRVs, every `BitmapFont::DrawString` allocates 1, every `SunBillboard::Render` allocates 1, and the `PostProcess::makeSRV` lambda creates **fresh SRVs for the same textures every frame** (see §3.1). A UI with lots of labels plus a few overlays will one day hit this and hard-crash the game.

**Recommendation:**

- Instrument: log high-watermark per frame in debug builds.
- Raise budget to 1 024 or make it configurable per scene.
- Replace `Neuron::Fatal` with graceful degradation: skip the draw and warn, or allocate a spill heap (though spill heaps require a new `SetDescriptorHeaps` call, which invalidates prior bindings — prefer a larger budget).

### 2.4 PSO/root-signature proliferation with no sharing

**Files:** `FlatColorPipeline.cpp`, `VertexColorPipeline.cpp`, `Starfield.cpp`, `SunBillboard.cpp`, `TacticalGrid.cpp`, `SpriteBatch.cpp`, `BitmapFont.cpp`, `PostProcess.cpp`.

Every renderer:

- Embeds its HLSL as a C++ string literal (several hundred lines per file).
- Calls `D3DCompile` at runtime during `Initialize()`.
- Defines a private `ID3D12RootSignature` that is structurally `{ b0 frame CBV, [b1 draw CBV], [table: t0, optional t1] }` — almost identical across renderers.
- Hand-rolls a `D3D12_GRAPHICS_PIPELINE_STATE_DESC` with 95% identical fields (reverse-Z, `GREATER_EQUAL`, back-buffer format, sample mask, etc.).

**Recommendations:**

1. **One shared root signature** (`CommonRootSig`) with slots `{ b0, b1, table(t0..t1) }` and a static sampler. All non-post-process renderers can use it; PSOs still swap input layouts, blend state, and shaders. This alone removes ~40 lines from each of the 7 renderers that define their own.
2. **Offline shader compilation.** Move the HLSL out of `.cpp`, compile to `.cso` at build time via `dxc`/`fxc`, embed with `CMake`’s `file(TO_C_HEADER)` or load from disk. Benefits:
   - Build-time validation (runtime compile errors become compile-time errors).
   - ~100 fewer lines in every pipeline `.cpp`.
   - Faster startup (no DXC spin-up for every PSO).
3. **PSO factory / builder.** A single `PsoBuilder` struct with fluent setters (`.WithVS(...).WithPS(...).WithInputLayout(...).WithBlend(Additive).WithDepth(ReverseZ)`) replaces every renderer’s `CreatePipelineState` boilerplate.

Estimated reduction: ~1 000–1 400 lines across the 8 pipeline files (~35–40% of the pipeline code).

---

## 3. High-priority Issues

### 3.1 Descriptors recreated every frame for static textures

**Files:**
- `GameRender/PostProcess.cpp:267-277` — `makeSRV` lambda inside the render function.
- `GameRender/SunBillboard.cpp:189-193` — copies `m_glowSRV_CPU` into the shader-visible heap each call.
- `GameRender/BitmapFont.cpp` — same pattern for the glyph atlas.

All three describe **static** resources: the back-buffer-format bloom SRVs alias the same textures every frame, and the sun glow / font atlas never change. Yet each frame we burn descriptor slots and either call `CreateShaderResourceView` or `CopyDescriptorsSimple` just to re-publish them.

**Recommendation:** Introduce a small **“persistent” segment** in the shader-visible heap (first N slots, set once at init, never reset by `BeginFrame`). Store GPU handles to pre-populated SRVs for the glow texture, font atlases, and the bloom RTs. The per-frame ring starts after the persistent segment.

### 3.2 Resource barriers are not batched

**File:** `GameRender/PostProcess.cpp:281-287, 313-324, 338-346, ...`

```cpp
auto barrier  = CD3DX12_RESOURCE_BARRIER::Transition(sceneRT, RT, SRV);
cmdList->ResourceBarrier(1, &barrier);
auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(m_brightPassRT.get(), SRV, RT);
cmdList->ResourceBarrier(1, &barrier2);
```

This is the canonical anti-pattern: two consecutive single-barrier calls between passes prevent the driver from merging them and cost an extra pipeline flush on some GPUs.

**Recommendation:** Batch all barriers for a pass boundary into one `ResourceBarrier(N, arr)` call. A tiny helper (`BarrierBatch` with `.Add(...)` / `.Flush(cmdList)`) makes this ergonomic and keeps call sites readable.

### 3.3 Root Signature version 1.0

**File:** `GameRender/PipelineHelpers.cpp:47` — `D3D_ROOT_SIGNATURE_VERSION_1`.

1.1 unlocks static-data and volatile-descriptor flags that let the driver optimize CBV/SRV access. It’s a one-line change with runtime feature-level detection and negligible risk.

**Recommendation:** Use `D3D12SerializeVersionedRootSignature` and prefer `VERSION_1_1` with a fallback to 1.0 if `CheckFeatureSupport(ROOT_SIGNATURE)` reports otherwise.

### 3.4 No device-removed handling

Nothing in `GameRender` (or visible in the `Core` interface surface it uses) checks `DXGI_ERROR_DEVICE_REMOVED` after `ExecuteCommandLists` / `Present`, and there’s no path to rebuild GPU-owned resources. A TDR or driver reset crashes the game with no telemetry.

**Recommendation:** After `Present` (in `Core`, likely), inspect the return; on device-removed, call `GetDeviceRemovedReason`, log it, and either exit cleanly or rebuild. GameRender renderers should expose a `RecreateGpuResources()` hook to support the latter.

### 3.5 `SurfaceRenderer::BuildSurfaceMesh` CPU work on first use

The unindex-plus-recolor path runs synchronously on the render thread the first time a unique (mesh, surface-type) pair is seen, then immediately calls the blocking `CreateStaticBuffer`. Combined with §2.2 this is a noticeable hitch.

**Recommendation:** Build surface meshes on a worker thread and hand completed vertex blobs to the async upload path. Cache key the same way.

---

## 4. Medium-priority Simplifications

### 4.1 Collapse `SpaceObjectRenderer` and `SurfaceRenderer`

They differ only in which pipeline they bind and the format of the draw CB. A single `MeshRenderer<Pipeline>` (or runtime `IMeshPipeline*` parameter) would eliminate one entire class and its header.

### 4.2 SpriteBatch / BitmapFont share 80% of their code

Both own a CPU vertex buffer, batch quads, allocate from the upload heap, draw in `FlushBatch`. Differences are (a) textured vs. solid-color PS and (b) input layout. Merge into a single `QuadBatch` with a pipeline selector.

### 4.3 `s_pso` / `s_rootSignature` static-per-class pattern in BitmapFont

**File:** `GameRender/BitmapFont.cpp:70-72`

Class-scope statics for GPU objects are fragile under teardown/device-recreate and surprising for callers (two `BitmapFont` instances quietly share state). Move to a `FontPipeline` singleton owned by the renderer or to the shared PSO registry from §2.4.

### 4.4 Drop duplicate `SetDescriptorHeaps` calls

`ApplyBloom` and `ApplyBloomAdditive` each bind the same heap (`PostProcess.cpp:258, 421`). Bind once per command list per heap change and assert in debug that no one else has replaced it.

### 4.5 Particle CB packing

If §2.1 is deferred: at least pack multiple particles per 256-byte CB (e.g. 4 per CB) to quarter the CB traffic without touching the draw model.

### 4.6 Unify `FrameConstants` across renderers

Each renderer declares its own `FrameConstants` struct (view-proj, camera, light). They are structurally identical. Define one canonical `FrameConstants` in a shared header, populate it once per frame in `GameApp::Render`, and pass a handle/GPU-VA to every renderer instead of each renderer reconstructing it.

### 4.7 Debug names and `SetName` coverage

Some objects have debug names (`GpuResourceManager`, `PostProcess`) but most descriptor heaps, meshes in `MeshCache`, and per-renderer PSOs don’t. Names are free at runtime and invaluable in PIX / Nsight captures. Add a small `NAME_D3D(obj, L"...")` macro and sprinkle it.

---

## 5. Lower-priority / Cosmetic

- **Topology in render loop** (`ParticleRenderer.cpp:54`) — set once before the loop.
- **Upload-heap size** — 4 MB × 3 is arbitrary; expose via config, log high-watermark per frame in debug.
- **Mesh-cache eviction** — `MeshCache` is append-only; for long sessions with many asteroids consider an LRU policy.
- **RTV descriptor churn on resize** — `PostProcess::OnResize` re-allocates RTVs but never releases the old ones back to the CPU heap. Low impact today; add a free-list if resizing becomes common.
- **`RECT` in a rendering API** (`SpriteBatch::DrawRect`) — prefer a float rect to avoid implicit int-to-float conversions at call sites.
- **Shader model 5.1 everywhere** — fine; only revisit if a feature genuinely needs SM 6.x.

---

## 6. Recommended Order of Work

| Order | Task | Risk | Payoff |
|---|---|---|---|
| 1 | §2.1 Particle instancing | Low | Large (perf + cleanliness) |
| 2 | §4.6 Unify `FrameConstants` | Low | Prep work for §2.4 |
| 3 | §2.4 Shared root sig + PSO builder + offline HLSL | Medium | Largest LOC reduction |
| 4 | §3.1 Persistent descriptor segment | Low | Removes per-frame descriptor churn |
| 5 | §3.2 Batched barriers | Low | Micro-perf + readability |
| 6 | §2.2 Async static uploads + §3.5 threaded surface-mesh build | Medium | Removes mid-game hitches |
| 7 | §2.3 Descriptor budget + diagnostics | Low | Stability |
| 8 | §4.1 / §4.2 Renderer mergers | Medium | Code reduction |
| 9 | §3.4 Device-removed handling | Medium | Robustness |
| 10 | §3.3 Root Sig 1.1 | Low | Minor perf / future-proofing |

---

## 7. Estimated Impact

- **Lines of code removed:** ~1 500–2 000 in `GameRender/` (pipeline files, duplicated FrameConstants, merged renderers) out of ~6 000, i.e. ~25–35%.
- **Draw-call reduction per frame at 2 000 particles:** ~2 000 → 1 (three orders of magnitude).
- **Per-frame upload-heap use at 2 000 particles:** ~512 KB → ~64 KB.
- **First-use hitches** from lazy surface meshes: eliminated.
- **Shader build-time errors:** caught at compile time instead of on first launch of that renderer.

---

## 8. What’s Already Good (keep it)

- Root-CBV binding strategy — optimal for small per-draw data.
- Ring-buffered upload heap with persistent mapping — textbook pattern, correctly implemented.
- Reverse-Z depth with `GREATER_EQUAL` — modern best practice.
- Starfield as a single static VB + one draw call — exemplary.
- CMO mesh caching in `SurfaceRenderer` — right idea, just needs the async completion path.
- Clear separation between `Core` (device/queue/swap chain) and `GameRender` (passes).

The foundation is solid; the work above is almost entirely about **removing duplication and adding two missing asynchrony paths** (static uploads and particle rendering), not rewriting the engine.
