#pragma once

#include "EntityHandle.h"
#include "ObjectDefs.h"

namespace Neuron
{
  // ShipLoadout — describes the equipped turrets on a ship.
  // Used by ShipLoadoutSystem to spawn and attach turret entities.
  struct ShipLoadout
  {
    uint16_t HullDefIndex  = 0;  // Index into ShipDef array
    uint16_t TurretDefs[MAX_TURRET_SLOTS] = {}; // Indices into TurretDef array
    uint8_t  TurretCount   = 0;
  };
}
