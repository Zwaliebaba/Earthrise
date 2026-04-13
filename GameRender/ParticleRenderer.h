#pragma once

// ParticleRenderer — renders billboard particles from ParticleSystem using
// a single instanced draw call.  Per-particle data is uploaded as a
// StructuredBuffer and read via SV_InstanceID in the vertex shader.

#include "ParticleSystem.h"
#include "Camera.h"
#include "ConstantBufferAllocator.h"
#include "ShaderVisibleHeap.h"
#include "UploadHeap.h"

namespace Neuron::Graphics
{
  class ParticleRenderer
  {
  public:
    void Initialize();

    // Render all live particles with a single instanced draw call.
    void Render(ID3D12GraphicsCommandList* _cmdList,
                UploadHeap& _uploadHeap,
                ConstantBufferAllocator& _cbAlloc,
                ShaderVisibleHeap& _srvHeap,
                const Camera& _camera,
                const ParticleSystem& _particles);

  private:
    com_ptr<ID3D12RootSignature> m_rootSignature;
    com_ptr<ID3D12PipelineState> m_pso;
  };
}
