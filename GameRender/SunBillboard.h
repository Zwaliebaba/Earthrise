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
    void Initialize(ShaderVisibleHeap& _srvHeap);

    void XM_CALLCONV Render(
      ID3D12GraphicsCommandList* _cmdList,
      ConstantBufferAllocator& _cbAlloc,
      ShaderVisibleHeap& _srvHeap,
      const Camera& _camera,
      FXMVECTOR _rebasedPosition,
      float _visualRadius,
      GXMVECTOR _color,
      float _gameTime);

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

    com_ptr<ID3D12Resource>      m_cloudyGlowTexture;
    D3D12_CPU_DESCRIPTOR_HANDLE  m_cloudyGlowSRV_CPU{};
    D3D12_GPU_DESCRIPTOR_HANDLE  m_cloudyGlowSRV_GPU{};

    com_ptr<ID3D12Resource>      m_starburstTexture;
    D3D12_CPU_DESCRIPTOR_HANDLE  m_starburstSRV_CPU{};
    D3D12_GPU_DESCRIPTOR_HANDLE  m_starburstSRV_GPU{};

    float m_fadeTimer{0.0f};
  };
}
