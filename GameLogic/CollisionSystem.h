#pragma once

#include "SpaceObjectManager.h"
#include "SpatialHash.h"

namespace GameLogic
{
  // Collision event — generated when two entities overlap.
  struct CollisionEvent
  {
    Neuron::EntityHandle A;
    Neuron::EntityHandle B;
  };

  // CollisionSystem — broad-phase spatial hash + narrow-phase sphere tests.
  // Generates collision events for the current tick.
  class CollisionSystem
  {
  public:
    CollisionSystem(SpaceObjectManager& _manager, SpatialHash& _spatialHash);

    // Rebuild the spatial hash from current entity positions.
    void RebuildSpatialHash();

    // Detect collisions between all active entities using broad+narrow phase.
    void DetectCollisions();

    // Get the collision events from the last DetectCollisions call.
    [[nodiscard]] const std::vector<CollisionEvent>& GetCollisions() const noexcept
    {
      return m_collisions;
    }

  private:
    // Narrow-phase: sphere-sphere overlap test.
    [[nodiscard]] static bool SphereSphereTest(
      FXMVECTOR _posA, float _radiusA,
      FXMVECTOR _posB, float _radiusB);

    SpaceObjectManager& m_manager;
    SpatialHash&        m_spatialHash;
    std::vector<CollisionEvent> m_collisions;
  };
}
