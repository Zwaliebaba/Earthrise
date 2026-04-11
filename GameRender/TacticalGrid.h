#pragma once

#include "PipelineHelpers.h"
#include "Camera.h"
#include "ConstantBufferAllocator.h"

namespace Neuron::Graphics
{
  // Optional wireframe reference plane for fleet command mode.
  // Rendered at a configurable Y-level with cyan/blue coloring and distance fade.
  class TacticalGrid
  {
  public:
    void Initialize();

    void Render(ID3D12GraphicsCommandList* cmdList,
      ConstantBufferAllocator& cbAlloc, const Camera& camera,
      float yLevel = 0.0f);

    void SetVisible(bool visible) noexcept { m_visible = visible; }
    [[nodiscard]] bool IsVisible() const noexcept { return m_visible; }
    void ToggleVisible() noexcept { m_visible = !m_visible; }

  private:
    bool m_visible = false;
    com_ptr<ID3D12Resource> m_vertexBuffer;
    com_ptr<ID3D12PipelineState> m_pso;
    com_ptr<ID3D12RootSignature> m_rootSignature;
    uint32_t m_lineVertexCount = 0;
  };
}
