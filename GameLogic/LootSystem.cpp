#include "pch.h"
#include "LootSystem.h"

namespace GameLogic
{
  LootSystem::LootSystem(SpaceObjectManager& _manager)
    : m_manager(_manager)
  {
  }

  void LootSystem::ProcessCollisions(const std::vector<CollisionEvent>& _collisions)
  {
    m_lootEvents.clear();

    for (const auto& col : _collisions)
    {
      Neuron::SpaceObject* objA = m_manager.GetSpaceObject(col.A);
      Neuron::SpaceObject* objB = m_manager.GetSpaceObject(col.B);
      if (!objA || !objB || !objA->Active || !objB->Active)
        continue;

      Neuron::SpaceObject* ship = nullptr;
      Neuron::SpaceObject* crate = nullptr;

      if (objA->Category == Neuron::SpaceObjectCategory::Ship &&
          objB->Category == Neuron::SpaceObjectCategory::Crate)
      {
        ship = objA;
        crate = objB;
      }
      else if (objB->Category == Neuron::SpaceObjectCategory::Ship &&
               objA->Category == Neuron::SpaceObjectCategory::Crate)
      {
        ship = objB;
        crate = objA;
      }
      else
      {
        continue;
      }

      Neuron::CrateData* crateData = m_manager.GetCrateData(crate->Handle);
      if (!crateData)
        continue;

      m_lootEvents.push_back({ ship->Handle, crate->Handle, crateData->LootTableId });

      // Destroy the crate.
      m_manager.DestroyEntity(crate->Handle);
    }
  }
}
