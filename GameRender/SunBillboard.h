#pragma once

#include "PipelineHelpers.h"
#include "ShaderVisibleHeap.h"
#include "ConstantBufferAllocator.h"
#include "Camera.h"

namespace Neuron::Graphics
{
  class SunBillboard
  {
  public:
    void Initialize();

    void XM_CALLCONV Render(
      ID3D12GraphicsCommandList* _cmdList,
      ConstantBufferAllocator& _cbAlloc,
      ShaderVisibleHeap& _srvHeap,
      const Camera& _camera,
      FXMVECTOR _rebasedPosition,
      float _visualRadius,
      FXMVECTOR _color);

  private:
    struct alignas(256) BillboardConstants
    {
      XMFLOAT4X4 ViewProjection;
      XMFLOAT3   CameraRight;
      float      Radius;
      XMFLOAT3   CameraUp;
      float      _Pad0;
      XMFLOAT3   Center;
      float      _Pad1;
      XMFLOAT4   Color;
    };

    com_ptr<ID3D12PipelineState> m_pso;
    com_ptr<ID3D12RootSignature> m_rootSignature;
    com_ptr<ID3D12Resource>      m_glowTexture;
    D3D12_CPU_DESCRIPTOR_HANDLE  m_glowSRV_CPU{};
  };
}
