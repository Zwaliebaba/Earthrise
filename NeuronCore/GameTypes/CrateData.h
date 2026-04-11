#pragma once

#include "EntityHandle.h"

namespace Neuron
{
  // CrateData — category-specific component for loot crates.
  struct CrateData
  {
    EntityHandle Owner;
    uint32_t     LootTableId  = 0;
    float        Lifetime     = 0.0f; // Seconds remaining before despawn (0 = infinite)
  };
}
