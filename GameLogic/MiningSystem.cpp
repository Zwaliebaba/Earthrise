#include "pch.h"
#include "MiningSystem.h"

namespace GameLogic
{
  MiningSystem::MiningSystem(SpaceObjectManager& _manager)
    : m_manager(_manager)
  {
  }

  void MiningSystem::ProcessCollisions(const std::vector<CollisionEvent>& _collisions)
  {
    m_depletions.clear();
    m_miningHits.clear();

    for (const auto& col : _collisions)
    {
      Neuron::SpaceObject* objA = m_manager.GetSpaceObject(col.A);
      Neuron::SpaceObject* objB = m_manager.GetSpaceObject(col.B);
      if (!objA || !objB || !objA->Active || !objB->Active)
        continue;

      // Identify projectile and asteroid.
      Neuron::SpaceObject* projectile = nullptr;
      Neuron::SpaceObject* asteroid   = nullptr;

      if (objA->Category == Neuron::SpaceObjectCategory::Projectile &&
          objB->Category == Neuron::SpaceObjectCategory::Asteroid)
      {
        projectile = objA;
        asteroid   = objB;
      }
      else if (objB->Category == Neuron::SpaceObjectCategory::Projectile &&
               objA->Category == Neuron::SpaceObjectCategory::Asteroid)
      {
        projectile = objB;
        asteroid   = objA;
      }
      else
      {
        continue;
      }

      // Get projectile and asteroid data.
      Neuron::ProjectileData* projData = m_manager.GetProjectileData(projectile->Handle);
      Neuron::AsteroidData* astData    = m_manager.GetAsteroidData(asteroid->Handle);
      if (!projData || !astData)
        continue;

      // Only mine asteroids that have resources.
      if (astData->ResourceAmount <= 0.0f)
        continue;

      // Extract resources: projectile damage acts as mining power.
      float extracted = std::min(projData->Damage, astData->ResourceAmount);
      astData->ResourceAmount -= extracted;

      // Mark this projectile as a mining hit (for despawning).
      m_miningHits.push_back(projectile->Handle);

      // Spawn a crate at the asteroid position.
      auto crateHandle = m_manager.CreateEntity(
        Neuron::SpaceObjectCategory::Crate, asteroid->Position, 0, 0);
      if (crateHandle.IsValid())
      {
        Neuron::CrateData* crate = m_manager.GetCrateData(crateHandle);
        if (crate)
        {
          crate->LootTableId = astData->ResourceType;
          crate->Lifetime    = 120.0f; // 2 minutes
        }
      }

      // Asteroid depleted?
      if (astData->ResourceAmount <= 0.0f)
      {
        m_depletions.push_back({
          asteroid->Handle,
          astData->ClusterId,
          asteroid->Position
        });
        m_manager.DestroyEntity(asteroid->Handle);
      }
    }
  }
}
