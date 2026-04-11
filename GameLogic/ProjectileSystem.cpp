#include "pch.h"
#include "ProjectileSystem.h"

namespace GameLogic
{
  ProjectileSystem::ProjectileSystem(SpaceObjectManager& _manager)
    : m_manager(_manager)
  {
  }

  void ProjectileSystem::Update(float _deltaTime)
  {
    m_expired.clear();

    m_manager.ForEachCategory(Neuron::SpaceObjectCategory::Projectile, [&](Neuron::SpaceObject& _obj)
    {
      Neuron::ProjectileData* proj = m_manager.GetProjectileData(_obj.Handle);
      if (!proj)
        return;

      proj->Lifetime -= _deltaTime;
      if (proj->Lifetime <= 0.0f)
      {
        m_expired.push_back(_obj.Handle);
      }
    });
  }

  void ProjectileSystem::DespawnProjectiles(const std::vector<Neuron::EntityHandle>& _hits)
  {
    // Destroy hit projectiles.
    for (auto handle : _hits)
    {
      if (m_manager.IsValid(handle))
        m_manager.DestroyEntity(handle);
    }

    // Destroy expired projectiles.
    for (auto handle : m_expired)
    {
      if (m_manager.IsValid(handle))
        m_manager.DestroyEntity(handle);
    }
  }
}
