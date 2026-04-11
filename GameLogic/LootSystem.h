#pragma once

#include "SpaceObjectManager.h"
#include "CollisionSystem.h"

namespace GameLogic
{
  // Loot pickup event — generated when a fleet ship collides with a crate.
  struct LootEvent
  {
    Neuron::EntityHandle Ship;
    Neuron::EntityHandle Crate;
    uint32_t             LootTableId = 0;
  };

  // LootSystem — detects ship-crate proximity from collision events,
  // destroys the crate, and generates loot pickup events.
  class LootSystem
  {
  public:
    explicit LootSystem(SpaceObjectManager& _manager);

    // Process collision events to find ship-crate overlaps. Destroys looted crates.
    void ProcessCollisions(const std::vector<CollisionEvent>& _collisions);

    // Get loot events from this tick.
    [[nodiscard]] const std::vector<LootEvent>& GetLootEvents() const noexcept { return m_lootEvents; }

  private:
    SpaceObjectManager& m_manager;
    std::vector<LootEvent> m_lootEvents;
  };
}
