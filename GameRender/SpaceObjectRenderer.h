#pragma once

#include "FlatColorPipeline.h"
#include "Camera.h"
#include "Mesh.h"

namespace Neuron::Graphics
{
  // Base renderer for all space objects.
  // Binds the flat-color pipeline, sets per-frame constants, then draws
  // individual objects with per-draw world transform and runtime-assigned color.
  class SpaceObjectRenderer
  {
  public:
    void Initialize();

    // Call once per frame before rendering any objects.
    void BeginFrame(ID3D12GraphicsCommandList* cmdList,
      ConstantBufferAllocator& cbAlloc, const Camera& camera);

    // Render a single object with the given mesh, world matrix, and color.
    void XM_CALLCONV RenderObject(ID3D12GraphicsCommandList* cmdList,
      ConstantBufferAllocator& cbAlloc,
      const Mesh* mesh, FXMMATRIX world, FXMVECTOR color);

    [[nodiscard]] FlatColorPipeline& GetPipeline() noexcept { return m_pipeline; }

  private:
    FlatColorPipeline m_pipeline;
  };
}
