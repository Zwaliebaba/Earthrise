#pragma once

#include "PipelineHelpers.h"
#include "ConstantBufferAllocator.h"

namespace Neuron::Graphics
{
  // Per-frame constants, bound to b0.
  struct alignas(256) FrameConstants
  {
    XMFLOAT4X4 ViewProjection;
    XMFLOAT3 CameraPosition;
    float _Pad0;
    XMFLOAT3 LightDirection;
    float AmbientIntensity;
  };

  // Per-draw constants, bound to b1.
  struct alignas(256) DrawConstants
  {
    XMFLOAT4X4 World;
    XMFLOAT4 Color;
  };

  // Root signature + PSO for flat-color rendering.
  class FlatColorPipeline
  {
  public:
    void Initialize();

    void BindPipeline(ID3D12GraphicsCommandList* cmdList) const;

    // Bind per-frame constants (call once per frame, after BindPipeline).
    void SetFrameConstants(ID3D12GraphicsCommandList* cmdList,
      ConstantBufferAllocator& cbAlloc,
      const FrameConstants& data) const;

    // Bind per-draw constants (call per object).
    void SetDrawConstants(ID3D12GraphicsCommandList* cmdList,
      ConstantBufferAllocator& cbAlloc,
      const DrawConstants& data) const;

    [[nodiscard]] ID3D12RootSignature* GetRootSignature() const noexcept { return m_rootSignature.get(); }
    [[nodiscard]] ID3D12PipelineState* GetPSO() const noexcept { return m_pso.get(); }

  private:
    com_ptr<ID3D12RootSignature> m_rootSignature;
    com_ptr<ID3D12PipelineState> m_pso;

    static constexpr UINT ROOT_PARAM_FRAME_CBV = 0; // b0
    static constexpr UINT ROOT_PARAM_DRAW_CBV = 1;  // b1
  };
}
