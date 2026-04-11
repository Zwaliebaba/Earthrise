#pragma once

#include "PipelineHelpers.h"
#include "Camera.h"
#include "ConstantBufferAllocator.h"

namespace Neuron::Graphics
{
  // Procedural star field — sparse point sprites on a dark background.
  // Stars are positioned on a large sphere around the camera (always visible).
  class Starfield
  {
  public:
    void Initialize(uint32_t starCount = 2000);
    void Render(ID3D12GraphicsCommandList* cmdList,
      ConstantBufferAllocator& cbAlloc, const Camera& camera);

  private:
    struct StarVertex
    {
      XMFLOAT3 Direction; // Unit direction from origin (camera-relative, always centered)
      float Brightness;
    };

    com_ptr<ID3D12Resource> m_vertexBuffer;
    com_ptr<ID3D12PipelineState> m_pso;
    com_ptr<ID3D12RootSignature> m_rootSignature;
    uint32_t m_starCount = 0;
  };
}
