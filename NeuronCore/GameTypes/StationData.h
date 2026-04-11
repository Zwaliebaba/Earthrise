#pragma once

#include "EntityHandle.h"

namespace Neuron
{
  // StationData — category-specific component for space stations.
  struct StationData
  {
    EntityHandle Owner;
    uint16_t     FactionId      = 0;
    float        DockingRadius  = 100.0f;
    bool         HasMarket      = false;
    bool         HasRepair      = false;
    bool         HasRefuel      = false;
    bool         HasMissions    = false;
  };
}
