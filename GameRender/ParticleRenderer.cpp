#include "pch.h"
#include "ParticleRenderer.h"

using namespace Neuron::Graphics;

void ParticleRenderer::Initialize()
{
  m_pipeline.Initialize();
}

void ParticleRenderer::Render(ID3D12GraphicsCommandList* _cmdList,
                               ConstantBufferAllocator& _cbAlloc,
                               const Camera& _camera,
                               const ParticleSystem& _particles)
{
  const auto& particles = _particles.GetParticles();
  if (particles.empty()) return;

  m_pipeline.BindPipeline(_cmdList);

  // Set frame constants (same pattern as SpaceObjectRenderer).
  FrameConstants fc{};
  XMStoreFloat4x4(&fc.ViewProjection, XMMatrixTranspose(_camera.GetViewProjectionMatrix()));
  XMStoreFloat3(&fc.CameraPosition, _camera.GetPosition());
  XMStoreFloat3(&fc.LightDirection, _camera.GetLightDirection());
  fc.AmbientIntensity = 1.0f; // Particles are self-lit.

  m_pipeline.SetFrameConstants(_cmdList, _cbAlloc, fc);

  XMVECTOR camPos = _camera.GetPosition();

  for (const auto& p : particles)
  {
    // Skip fully transparent particles.
    if (p.Color.w <= 0.0f) continue;

    // Origin-rebased position.
    XMVECTOR worldPos = XMLoadFloat3(&p.Position);
    XMVECTOR rebased  = XMVectorSubtract(worldPos, XMVectorSetW(camPos, 0.0f));

    // Build a uniform scale matrix at the rebased position.
    XMMATRIX world = XMMatrixScaling(p.Size, p.Size, p.Size);
    world.r[3] = XMVectorSetW(rebased, 1.0f);

    DrawConstants dc{};
    XMStoreFloat4x4(&dc.World, XMMatrixTranspose(world));
    dc.Color = p.Color;

    m_pipeline.SetDrawConstants(_cmdList, _cbAlloc, dc);

    // Draw a point-like particle as a degenerate triangle.
    // Without a dedicated particle mesh, we use DrawInstanced with 1 vertex.
    // The vertex shader will expand it using the draw-constant world matrix.
    _cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
    _cmdList->DrawInstanced(1, 1, 0, 0);
  }
}
