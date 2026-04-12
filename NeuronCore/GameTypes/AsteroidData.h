#pragma once

#include "EntityHandle.h"

namespace Neuron
{
  // AsteroidData — category-specific component for asteroids.
  // Stored in a parallel array keyed by EntityHandle.
  struct AsteroidData
  {
    EntityHandle Owner;               // Handle back to SpaceObject
    float        RotationSpeed = 0.0f; // Radians per second (tumble)
    XMFLOAT3     RotationAxis  = { 0.0f, 1.0f, 0.0f };
    uint32_t     ResourceType  = 0;   // Mineable resource type (0 = none)
    float        ResourceAmount = 0.0f;
    uint16_t     ClusterId     = 0;   // Parent cluster index (0 = no cluster)
    float        MaxResource   = 0.0f; // Initial resource cap (for regrowth)
  };
}
