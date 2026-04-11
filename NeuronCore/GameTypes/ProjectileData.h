#pragma once

#include "EntityHandle.h"

namespace Neuron
{
  // ProjectileData — category-specific component for projectiles (missiles, rockets, alien seekers).
  struct ProjectileData
  {
    EntityHandle Owner;          // The projectile entity itself
    EntityHandle Source;         // Ship that fired this projectile
    EntityHandle Target;         // Homing target (invalid if unguided)
    float        Lifetime  = 0.0f; // Seconds remaining
    float        Damage    = 0.0f;
    float        Speed     = 0.0f;
    float        TurnRate  = 0.0f; // Homing turn rate (0 = straight line)
  };
}
