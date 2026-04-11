#pragma once

#include "EntityHandle.h"

namespace Neuron
{
  // DecorationData — category-specific component for inert decorative objects.
  struct DecorationData
  {
    EntityHandle Owner;
    float        RotationSpeed = 0.0f;
    XMFLOAT3     RotationAxis  = { 0.0f, 1.0f, 0.0f };
  };
}
