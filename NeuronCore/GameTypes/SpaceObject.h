#pragma once

#include "EntityHandle.h"
#include "SpaceObjectCategory.h"

namespace Neuron
{
  // SpaceObject — common component shared by all entity categories.
  // Flat POD-like struct: no virtual methods, no inheritance, cache-friendly for iteration.
  // Stored in a contiguous array indexed by EntityHandle slot.
  struct SpaceObject
  {
    EntityHandle         Handle;
    SpaceObjectCategory  Category      = SpaceObjectCategory::SpaceObject;
    bool                 Active        = false;

    // Transform
    XMFLOAT3             Position      = { 0.0f, 0.0f, 0.0f };
    XMFLOAT4             Orientation   = { 0.0f, 0.0f, 0.0f, 1.0f }; // Quaternion (identity)

    // Physics
    XMFLOAT3             Velocity      = { 0.0f, 0.0f, 0.0f };
    float                BoundingRadius = 1.0f;

    // Appearance
    XMFLOAT4             Color         = { 1.0f, 1.0f, 1.0f, 1.0f }; // Runtime-assigned
    uint32_t             MeshNameHash  = 0;  // Hash of mesh filename for MeshCache lookup

    // Definition index — references the ObjectDef that spawned this entity
    uint16_t             DefIndex      = 0;
  };
}
