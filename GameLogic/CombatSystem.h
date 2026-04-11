#pragma once

#include "SpaceObjectManager.h"
#include "MovementSystem.h"

namespace GameLogic
{
  // CombatSystem — manages ship attack targets, turret auto-fire, projectile spawning,
  // and damage application from collision events.
  class CombatSystem
  {
  public:
    CombatSystem(SpaceObjectManager& _manager, MovementSystem& _movement);

    // Set a ship's attack target (from player command or AI).
    void SetAttackTarget(Neuron::EntityHandle _ship, Neuron::EntityHandle _target);

    // Clear attack target.
    void ClearAttackTarget(Neuron::EntityHandle _ship);

    // Get the attack target for a ship (Invalid if none).
    [[nodiscard]] Neuron::EntityHandle GetAttackTarget(Neuron::EntityHandle _ship) const;

    // Tick: turret tracking, cooldown, auto-fire, energy drain.
    void Update(float _deltaTime);

    // Apply damage from projectile collisions. Call after CollisionSystem::DetectCollisions.
    // Returns handles of ships destroyed this tick.
    struct DamageResult
    {
      Neuron::EntityHandle Destroyed; // Ship that was destroyed
      XMFLOAT3             Position;  // Where it died (for crate spawning)
    };
    void ProcessCollisions(const std::vector<struct CollisionEvent>& _collisions);

    // Get ships destroyed this tick (populated by ProcessCollisions).
    [[nodiscard]] const std::vector<DamageResult>& GetDestroyedShips() const noexcept { return m_destroyed; }

    // Get projectiles that hit this tick (for despawning).
    [[nodiscard]] const std::vector<Neuron::EntityHandle>& GetProjectileHits() const noexcept { return m_projectileHits; }

  private:
    // Spawn a projectile from a turret.
    void SpawnProjectile(Neuron::EntityHandle _turret, Neuron::EntityHandle _target,
      const Neuron::TurretData& _turretData, const XMFLOAT3& _spawnPos);

    // Apply damage to a ship's shields → armor → hull.
    // Returns true if ship is destroyed.
    bool ApplyDamage(Neuron::ShipData& _ship, float _damage);

    SpaceObjectManager& m_manager;
    MovementSystem&     m_movement;

    // Per-entity attack target. Parallel to entity array.
    struct AttackState
    {
      Neuron::EntityHandle Target;
    };
    std::vector<AttackState> m_attackStates;

    std::vector<DamageResult>        m_destroyed;
    std::vector<Neuron::EntityHandle> m_projectileHits;
  };
}
