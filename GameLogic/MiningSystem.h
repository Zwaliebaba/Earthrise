#pragma once

#include "SpaceObjectManager.h"
#include "CollisionSystem.h"

namespace GameLogic
{
  // MiningSystem — processes projectile-asteroid collisions to extract resources.
  // Spawns crates and emits depletion events for the regrowth system.
  class MiningSystem
  {
  public:
    explicit MiningSystem(SpaceObjectManager& _manager);

    struct DepletionEvent
    {
      Neuron::EntityHandle Asteroid;
      uint16_t             ClusterId = 0;
      XMFLOAT3             Position  = {};
    };

    // Process collision events. Call after CombatSystem::ProcessCollisions.
    void ProcessCollisions(const std::vector<CollisionEvent>& _collisions);

    [[nodiscard]] const std::vector<DepletionEvent>& GetDepletions() const noexcept { return m_depletions; }

    // Projectile handles that hit asteroids this tick (for despawning).
    [[nodiscard]] const std::vector<Neuron::EntityHandle>& GetMiningHits() const noexcept { return m_miningHits; }

  private:
    SpaceObjectManager& m_manager;
    std::vector<DepletionEvent> m_depletions;
    std::vector<Neuron::EntityHandle> m_miningHits;
  };
}
