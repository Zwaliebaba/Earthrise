#pragma once

#include "EntityHandle.h"

namespace Neuron
{
  static constexpr uint32_t MAX_TURRET_SLOTS = 6;

  // ShipData — category-specific component for player and NPC ships.
  struct ShipData
  {
    EntityHandle Owner;

    // Durability
    float ShieldHP     = 0.0f;
    float ShieldMaxHP  = 0.0f;
    float ArmorHP      = 0.0f;
    float ArmorMaxHP   = 0.0f;
    float HullHP       = 0.0f;
    float HullMaxHP    = 0.0f;

    // Energy
    float Energy       = 0.0f;
    float EnergyMax    = 0.0f;
    float EnergyRegen  = 0.0f; // Per second

    // Movement
    float MaxSpeed     = 0.0f;
    float TurnRate     = 0.0f; // Radians per second
    float Thrust       = 0.0f;

    // Turret slots — handles to attached turret entities
    EntityHandle TurretSlots[MAX_TURRET_SLOTS] = {};
    uint8_t      TurretSlotCount = 0;

    // Faction / AI
    uint16_t     FactionId  = 0;
    bool         IsPlayer   = false;
  };
}
