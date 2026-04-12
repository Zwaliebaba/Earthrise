#pragma once

#include "PipelineHelpers.h"
#include "ConstantBufferAllocator.h"
#include "FlatColorPipeline.h"

namespace Neuron::Graphics
{
  // Vertex with per-vertex color, used by SurfaceRenderer for landscape-colored meshes.
  struct SurfaceVertex
  {
    XMFLOAT3 Position;
    XMFLOAT3 Normal;
    XMFLOAT4 Color;
  };

  // Per-draw constants for vertex-color pipeline — world only (color comes from vertices).
  struct alignas(256) SurfaceDrawConstants
  {
    XMFLOAT4X4 World;
  };

  // Root signature + PSO for per-vertex-colored rendering.
  // Uses the same FrameConstants (b0) as FlatColorPipeline.
  // DrawConstants (b1) contains only the world matrix; color is per-vertex.
  class VertexColorPipeline
  {
  public:
    void Initialize();

    void BindPipeline(ID3D12GraphicsCommandList* cmdList) const;

    // Bind per-frame constants (call once per frame, after BindPipeline).
    // Reuses FrameConstants from FlatColorPipeline.
    void SetFrameConstants(ID3D12GraphicsCommandList* cmdList,
      ConstantBufferAllocator& cbAlloc,
      const FrameConstants& data) const;

    // Bind per-draw constants (call per object).
    void SetDrawConstants(ID3D12GraphicsCommandList* cmdList,
      ConstantBufferAllocator& cbAlloc,
      const SurfaceDrawConstants& data) const;

    [[nodiscard]] ID3D12RootSignature* GetRootSignature() const noexcept { return m_rootSignature.get(); }
    [[nodiscard]] ID3D12PipelineState* GetPSO() const noexcept { return m_pso.get(); }

  private:
    com_ptr<ID3D12RootSignature> m_rootSignature;
    com_ptr<ID3D12PipelineState> m_pso;

    static constexpr UINT ROOT_PARAM_FRAME_CBV = 0; // b0
    static constexpr UINT ROOT_PARAM_DRAW_CBV  = 1; // b1
  };
}
