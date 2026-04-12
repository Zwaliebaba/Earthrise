#pragma once

#include "SpaceObjectCategory.h"
#include "SurfaceType.h"

namespace Neuron
{
  // ObjectDef — data-driven template for spawning space objects.
  // Defines the baseline stats and mesh for each object type.
  // Shared between client (for deserialization/rendering) and server (for spawning).
  // Loaded from binary definition files on the server; client receives mesh name via spawn messages.

  // Base definition common to all categories.
  struct ObjectDefBase
  {
    uint16_t             DefId         = 0;
    SpaceObjectCategory  Category      = SpaceObjectCategory::SpaceObject;
    std::string          Name;                // Human-readable name (e.g., "Asteroid01")
    std::string          MeshName;            // Mesh filename without extension (e.g., "Asteroid01")
    uint32_t             MeshNameHash  = 0;   // Pre-computed hash of MeshName
    float                BoundingRadius = 1.0f;
    XMFLOAT4             DefaultColor  = { 1.0f, 1.0f, 1.0f, 1.0f };
  };

  // Asteroid definition
  struct AsteroidDef : ObjectDefBase
  {
    float       MinRotationSpeed = 0.0f;
    float       MaxRotationSpeed = 0.5f;
    uint32_t    ResourceType     = 0;
    float       MaxResource      = 0.0f;
    SurfaceType Surface          = SurfaceType::Default;
  };

  // Crate definition
  struct CrateDef : ObjectDefBase
  {
    uint32_t LootTableId = 0;
    float    Lifetime    = 300.0f; // Default 5 minutes
  };

  // Decoration definition
  struct DecorationDef : ObjectDefBase
  {
    float MinRotationSpeed = 0.0f;
    float MaxRotationSpeed = 0.1f;
  };

  // Ship definition
  struct ShipDef : ObjectDefBase
  {
    float   ShieldMaxHP  = 100.0f;
    float   ArmorMaxHP   = 100.0f;
    float   HullMaxHP    = 100.0f;
    float   EnergyMax    = 100.0f;
    float   EnergyRegen  = 5.0f;
    float   MaxSpeed     = 50.0f;
    float   TurnRate     = 1.0f;
    float   Thrust       = 20.0f;
    uint8_t TurretSlots  = 0;
  };

  // Jumpgate definition
  struct JumpgateDef : ObjectDefBase
  {
    float ActivationRadius = 50.0f;
  };

  // Projectile definition
  struct ProjectileDef : ObjectDefBase
  {
    float Damage    = 10.0f;
    float Speed     = 200.0f;
    float Lifetime  = 5.0f;
    float TurnRate  = 0.0f; // 0 = unguided
  };

  // Station definition
  struct StationDef : ObjectDefBase
  {
    float DockingRadius = 100.0f;
    bool  HasMarket     = false;
    bool  HasRepair     = false;
    bool  HasRefuel     = false;
    bool  HasMissions   = false;
  };

  // Turret definition
  struct TurretDef : ObjectDefBase
  {
    float FireRate        = 1.0f;
    float Damage          = 10.0f;
    float Range           = 500.0f;
    float ProjectileSpeed = 200.0f;
    float TrackingSpeed   = 2.0f;
  };

  // Planet definition
  struct PlanetDef : ObjectDefBase
  {
    float       MinOrbitRadius    = 10000.0f;
    float       MaxOrbitRadius    = 40000.0f;
    float       MinRotationSpeed  = 0.01f;
    float       MaxRotationSpeed  = 0.1f;
    SurfaceType Surface           = SurfaceType::Default;
  };

  // Sun definition
  struct SunDef : ObjectDefBase
  {
    float MinLuminosity  = 0.8f;
    float MaxLuminosity  = 1.5f;
    float MinVisualRadius = 500.0f;
    float MaxVisualRadius = 1200.0f;
  };

  // Simple FNV-1a hash for mesh name strings.
  [[nodiscard]] constexpr uint32_t HashMeshName(std::string_view _name) noexcept
  {
    uint32_t hash = 2166136261u;
    for (char c : _name)
    {
      hash ^= static_cast<uint32_t>(c);
      hash *= 16777619u;
    }
    return hash;
  }
}
