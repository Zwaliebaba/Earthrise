#pragma once

#include "EntityHandle.h"

namespace Neuron
{
  // JumpgateData — category-specific component for jump gates.
  struct JumpgateData
  {
    EntityHandle Owner;
    uint32_t     DestinationZoneId = 0;
    XMFLOAT3     DestinationPosition = { 0.0f, 0.0f, 0.0f };
    float        ActivationRadius = 50.0f;
    bool         IsActive = true;
  };
}
