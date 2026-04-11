#pragma once

#include "SpaceObjectManager.h"

namespace GameLogic
{
  // Per-entity movement command state. Stored in parallel array keyed by entity slot.
  struct MovementCommand
  {
    bool     HasTarget         = false;
    XMFLOAT3 TargetPosition    = {};
    float    ArrivalThreshold  = 5.0f; // Distance at which we consider "arrived"
  };

  // MovementSystem — full 3D steering/arrival behavior for ships and projectiles.
  // Integrates velocity, rotates toward target, clamps speed. Server-only.
  class MovementSystem
  {
  public:
    explicit MovementSystem(SpaceObjectManager& _manager);

    // Set a move-to target for a ship entity.
    void SetMoveTarget(Neuron::EntityHandle _handle, const XMFLOAT3& _target);

    // Clear movement target (stop moving).
    void ClearMoveTarget(Neuron::EntityHandle _handle);

    // Check if entity has an active movement target.
    [[nodiscard]] bool HasMoveTarget(Neuron::EntityHandle _handle) const;

    // Tick: integrate velocity, steer toward targets, apply arrival behavior.
    void Update(float _deltaTime);

  private:
    void UpdateShip(Neuron::SpaceObject& _obj, Neuron::ShipData& _ship, float _dt);
    void UpdateProjectile(Neuron::SpaceObject& _obj, Neuron::ProjectileData& _proj, float _dt);

    SpaceObjectManager& m_manager;
    std::vector<MovementCommand> m_commands; // Parallel to SpaceObjectManager's entity array
  };
}
