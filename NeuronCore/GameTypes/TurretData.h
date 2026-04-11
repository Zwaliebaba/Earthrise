#pragma once

#include "EntityHandle.h"

namespace Neuron
{
  // TurretData — category-specific component for turrets (mounted on ships or stations).
  struct TurretData
  {
    EntityHandle Owner;          // The turret entity itself
    EntityHandle ParentShip;     // Ship/station this turret is mounted on
    EntityHandle Target;         // Current target

    // Weapon stats
    float        FireRate        = 1.0f; // Shots per second
    float        Damage          = 0.0f;
    float        Range           = 0.0f;
    float        ProjectileSpeed = 0.0f;
    float        CooldownTimer   = 0.0f; // Seconds until next shot

    // Turret rotation
    float        YawAngle        = 0.0f;
    float        PitchAngle      = 0.0f;
    float        TrackingSpeed   = 0.0f; // Radians per second
  };
}
