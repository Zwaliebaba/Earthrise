#pragma once

#include "VertexColorPipeline.h"
#include "LandscapeTexture.h"
#include "CmoLoader.h"
#include "Camera.h"
#include "GpuResourceManager.h"

namespace Neuron::Graphics
{
  // GPU-resident mesh with per-vertex landscape colors baked in.
  struct SurfaceMesh
  {
    com_ptr<ID3D12Resource> VertexBuffer;
    com_ptr<ID3D12Resource> IndexBuffer;
    uint32_t VertexCount = 0;
    uint32_t IndexCount  = 0;
    uint32_t VertexStride = sizeof(SurfaceVertex);
    std::vector<CmoSubmesh> Submeshes;

    [[nodiscard]] D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const noexcept
    {
      D3D12_VERTEX_BUFFER_VIEW view{};
      if (VertexBuffer)
      {
        view.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
        view.SizeInBytes    = VertexCount * VertexStride;
        view.StrideInBytes  = VertexStride;
      }
      return view;
    }

    [[nodiscard]] D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const noexcept
    {
      D3D12_INDEX_BUFFER_VIEW view{};
      if (IndexBuffer)
      {
        view.BufferLocation = IndexBuffer->GetGPUVirtualAddress();
        view.SizeInBytes    = IndexCount * sizeof(uint16_t);
        view.Format         = DXGI_FORMAT_R16_UINT;
      }
      return view;
    }
  };

  // Renders meshes with per-vertex colors sampled from landscape gradient textures.
  // Used for asteroids and planets to produce Darwinia-style terrain coloring.
  //
  // Workflow:
  //   1. Initialize() — loads landscape textures, creates pipeline
  //   2. GetSurfaceMesh() — produces a colored mesh (cached per meshKey + surfaceType)
  //   3. BeginFrame() — binds pipeline and frame constants
  //   4. RenderObject() — draws a surface-colored mesh with a world transform
  class SurfaceRenderer
  {
  public:
    void Initialize();

    // Call once per frame before rendering surface objects.
    void BeginFrame(ID3D12GraphicsCommandList* cmdList,
      ConstantBufferAllocator& cbAlloc, const Camera& camera);

    // Get (or build) a surface-colored mesh. Thread-safe for single-threaded rendering.
    // meshKey: MeshCache-style key (e.g., "Asteroids/Asteroid01")
    [[nodiscard]] SurfaceMesh* GetSurfaceMesh(std::string_view meshKey,
                                              Neuron::SurfaceType surfaceType);

    // Render a surface-colored mesh with the given world transform.
    void XM_CALLCONV RenderObject(ID3D12GraphicsCommandList* cmdList,
      ConstantBufferAllocator& cbAlloc,
      const SurfaceMesh* mesh, FXMMATRIX world);

  private:
    VertexColorPipeline m_pipeline;

    // One landscape texture per SurfaceType
    std::unique_ptr<LandscapeTexture> m_textures[static_cast<size_t>(Neuron::SurfaceType::COUNT)];

    // Cache: hash(meshKey + surfaceType) → SurfaceMesh
    std::unordered_map<uint64_t, std::unique_ptr<SurfaceMesh>> m_meshCache;

    // Build a surface mesh from CMO data + landscape texture
    std::unique_ptr<SurfaceMesh> BuildSurfaceMesh(const CmoMeshData& meshData,
                                                  Neuron::SurfaceType surfaceType);

    // Assign per-vertex colors from landscape texture sampling
    void BuildColorArray(std::vector<SurfaceVertex>& verts,
                         const std::vector<uint16_t>& indices,
                         const LandscapeTexture* texture, float highest);

    // Sample a color from the landscape texture based on height and gradient
    XMFLOAT4 GetLandscapeColour(float height, float gradient,
                                unsigned int x, unsigned int y,
                                const LandscapeTexture* texture, float highest);

    // Darwinia-style deterministic random (for per-vertex noise)
    uint32_t m_rngState = 12345;
    void SeedRandom(unsigned int seed);
    [[nodiscard]] uint32_t Random();
    [[nodiscard]] float SignedFrand(float range);

    // Compute cache key from mesh key + surface type
    [[nodiscard]] static uint64_t ComputeCacheKey(std::string_view meshKey,
                                                  Neuron::SurfaceType surfaceType) noexcept;
  };
}
