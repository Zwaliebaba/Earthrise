#pragma once

#include "SpaceObjectRenderer.h"
#include "MeshCache.h"
#include "SurfaceRenderer.h"

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

  class AsteroidRenderer final : public CategoryRenderer
  {
  public:
    void SetSurfaceRenderer(SurfaceRenderer* surfaceRenderer) noexcept
    {
      m_surfaceRenderer = surfaceRenderer;
    }

    // Render an asteroid with landscape-colored surface.
    void XM_CALLCONV RenderSurfaceObject(ID3D12GraphicsCommandList* cmdList,
      ConstantBufferAllocator& cbAlloc,
      std::string_view meshKey, Neuron::SurfaceType surfaceType, FXMMATRIX world)
    {
      if (!m_surfaceRenderer) return;
      SurfaceMesh* surfMesh = m_surfaceRenderer->GetSurfaceMesh(meshKey, surfaceType);
      if (surfMesh)
        m_surfaceRenderer->RenderObject(cmdList, cbAlloc, surfMesh, world);
    }

  private:
    SurfaceRenderer* m_surfaceRenderer = nullptr;
  };
  class ShipRenderer final : public CategoryRenderer {};
  class StationRenderer final : public CategoryRenderer {};
  class JumpgateRenderer final : public CategoryRenderer {};
  class CrateRenderer final : public CategoryRenderer {};
  class DecorationRenderer final : public CategoryRenderer {};
  class ProjectileRenderer final : public CategoryRenderer {};
  class TurretRenderer final : public CategoryRenderer {};
}
