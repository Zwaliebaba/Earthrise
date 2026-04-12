#pragma once

#include "EntityHandle.h"

namespace Neuron
{
  struct SunData
  {
    EntityHandle Owner;
    float        Luminosity     = 1.0f;
    float        VisualRadius   = 800.0f;
    float        RotationSpeed  = 0.0f;
    XMFLOAT3     RotationAxis   = { 0.0f, 1.0f, 0.0f };
  };
}
