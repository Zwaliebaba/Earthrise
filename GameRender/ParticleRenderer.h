#pragma once

// ParticleRenderer — renders flat-color billboard particles from ParticleSystem
// using the existing FlatColorPipeline.  Each particle is drawn as a
// camera-facing quad with origin-rebased position.

#include "ParticleSystem.h"
#include "FlatColorPipeline.h"
#include "Camera.h"
#include "ConstantBufferAllocator.h"

namespace Neuron::Graphics
{
  class ParticleRenderer
  {
  public:
    void Initialize();

    // Render all live particles.
    void Render(ID3D12GraphicsCommandList* _cmdList,
                ConstantBufferAllocator& _cbAlloc,
                const Camera& _camera,
                const ParticleSystem& _particles);

  private:
    FlatColorPipeline m_pipeline;
  };
}
