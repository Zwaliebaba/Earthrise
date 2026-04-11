#pragma once

#include "SpaceObjectRenderer.h"
#include "MeshCache.h"

namespace Neuron::Graphics
{
  // Per-category renderer base. Each category renderer wraps SpaceObjectRenderer
  // with category-specific defaults and can be extended with gameplay-specific
  // rendering in later phases (e.g., turret rotation, engine glow).
  class CategoryRenderer
  {
  public:
    virtual ~CategoryRenderer() = default;

    virtual void Initialize(SpaceObjectRenderer* renderer, MeshCache* meshCache)
    {
      m_renderer = renderer;
      m_meshCache = meshCache;
    }

    void XM_CALLCONV RenderObject(ID3D12GraphicsCommandList* cmdList,
      ConstantBufferAllocator& cbAlloc,
      const Mesh* mesh, FXMMATRIX world, FXMVECTOR color)
    {
      m_renderer->RenderObject(cmdList, cbAlloc, mesh, world, color);
    }

  protected:
    SpaceObjectRenderer* m_renderer = nullptr;
    MeshCache* m_meshCache = nullptr;
  };

  class AsteroidRenderer final : public CategoryRenderer {};
  class ShipRenderer final : public CategoryRenderer {};
  class StationRenderer final : public CategoryRenderer {};
  class JumpgateRenderer final : public CategoryRenderer {};
  class CrateRenderer final : public CategoryRenderer {};
  class DecorationRenderer final : public CategoryRenderer {};
  class ProjectileRenderer final : public CategoryRenderer {};
  class TurretRenderer final : public CategoryRenderer {};
}
