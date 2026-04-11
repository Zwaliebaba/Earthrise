#pragma once

// SpaceObjectCategory — enumerates all entity categories for the space game.
// Populated in Phase 2.

namespace Neuron
{
  enum class SpaceObjectCategory : uint8_t
  {
    SpaceObject,  // Generic base / catch-all
    Asteroid,
    Crate,
    Decoration,
    Ship,
    Jumpgate,
    Projectile,
    Station,
    Turret,

    COUNT         // Sentinel for ENUM_HELPER
  };
  ENUM_HELPER(SpaceObjectCategory, SpaceObject, Turret)
}
