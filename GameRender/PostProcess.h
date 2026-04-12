#pragma once

#include "PipelineHelpers.h"
#include "ShaderVisibleHeap.h"
#include "ConstantBufferAllocator.h"

namespace Neuron::Graphics
{
  // Bloom post-processing: extract bright pixels, Gaussian blur, additive composite.
  // Critical for the Darwinia glow aesthetic on weapons/engines/explosions.
  class PostProcess
  {
  public:
    void Initialize(UINT width, UINT height);
    void OnResize(UINT width, UINT height);

    // Apply bloom to the current render target.
    // sceneRTHandle: SRV descriptor for the scene render target
    void ApplyBloom(ID3D12GraphicsCommandList* cmdList,
      ConstantBufferAllocator& cbAlloc,
      ShaderVisibleHeap& srvHeap,
      ID3D12Resource* sceneRT,
      float threshold = 0.8f);

    // LDR-compatible bloom: extract + blur + additive composite onto back buffer.
    // Does NOT require a scene RT as SRV — bloom is additively blended on top.
    void ApplyBloomAdditive(ID3D12GraphicsCommandList* cmdList,
      ConstantBufferAllocator& cbAlloc,
      ShaderVisibleHeap& srvHeap,
      ID3D12Resource* sceneRT,
      float threshold = 0.65f) const;

  private:
    void CreateResources(UINT width, UINT height);

    com_ptr<ID3D12Resource> m_brightPassRT;
    com_ptr<ID3D12Resource> m_blurRT[2]; // Ping-pong
    D3D12_CPU_DESCRIPTOR_HANDLE m_brightPassRTV{};
    D3D12_CPU_DESCRIPTOR_HANDLE m_blurRTV[2]{};

    com_ptr<ID3D12RootSignature> m_rootSignature;
    com_ptr<ID3D12PipelineState> m_extractPSO;
    com_ptr<ID3D12PipelineState> m_blurPSO;
    com_ptr<ID3D12PipelineState> m_compositePSO;
    com_ptr<ID3D12PipelineState> m_compositeAdditivePSO;

    UINT m_width = 0;
    UINT m_height = 0;

    static constexpr UINT BLOOM_DOWNSCALE = 2; // Half-res blur for performance
  };
}
