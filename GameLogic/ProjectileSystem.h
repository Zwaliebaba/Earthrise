#pragma once

#include "SpaceObjectManager.h"

namespace GameLogic
{
  // ProjectileSystem — manages projectile lifetime and despawning.
  // Projectile movement is handled by MovementSystem.
  // Projectile-ship collision damage is handled by CombatSystem.
  class ProjectileSystem
  {
  public:
    explicit ProjectileSystem(SpaceObjectManager& _manager);

    // Tick: decrement lifetime, collect expired projectiles.
    void Update(float _deltaTime);

    // Despawn projectiles that hit targets (called after CombatSystem).
    void DespawnProjectiles(const std::vector<Neuron::EntityHandle>& _hits);

    // Get projectiles that expired this tick (for caller to destroy).
    [[nodiscard]] const std::vector<Neuron::EntityHandle>& GetExpired() const noexcept { return m_expired; }

  private:
    SpaceObjectManager& m_manager;
    std::vector<Neuron::EntityHandle> m_expired;
  };
}
