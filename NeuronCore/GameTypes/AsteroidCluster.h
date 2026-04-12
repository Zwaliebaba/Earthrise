#pragma once

#include "EntityHandle.h"

namespace Neuron
{
  enum class ClusterShape : uint8_t
  {
    Ring,       // Torus around a parent celestial body
    Supernova,  // Irregular spherical debris cloud
    COUNT
  };
  ENUM_HELPER(ClusterShape, Ring, Supernova)

  // AsteroidCluster — describes the procedural shape and resource profile of one cluster.
  // Not an entity — metadata owned by UniverseGenerator / Zone.
  struct AsteroidCluster
  {
    uint16_t      ClusterId       = 0;
    ClusterShape  Shape           = ClusterShape::Supernova;

    // Spatial definition
    XMFLOAT3      Center          = {};
    XMFLOAT4      Orientation     = { 0.0f, 0.0f, 0.0f, 1.0f }; // Tilt of the ring plane
    float         InnerRadius     = 0.0f;            // Ring: hole radius; Supernova: min spawn radius
    float         OuterRadius     = 0.0f;            // Outer extent
    float         Thickness       = 0.0f;            // Ring: vertical thickness; Supernova: unused

    // Content
    uint32_t      AsteroidCount   = 0;               // Target count when fully populated
    uint32_t      ResourceType    = 0;
    float         MinResource     = 0.0f;
    float         MaxResource     = 0.0f;
    float         MinRadius       = 0.5f;            // Bounding radius range for asteroids
    float         MaxRadius       = 3.0f;

    // Regrowth
    float         RespawnInterval = 300.0f;          // Seconds before a depleted slot regrows
    EntityHandle  ParentBody;                        // Planet or Sun this cluster orbits (Invalid = free-floating)

    // Runtime bookkeeping (not serialized in seed — stored in delta)
    uint32_t      ActiveCount     = 0;
    struct RegrowthEntry
    {
      float Timer = 0.0f;                            // Seconds remaining
    };
    std::vector<RegrowthEntry> RegrowthQueue;
  };
}
