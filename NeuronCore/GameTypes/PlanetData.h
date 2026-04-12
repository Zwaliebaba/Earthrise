#pragma once

#include "EntityHandle.h"

namespace Neuron
{
  struct PlanetData
  {
    EntityHandle Owner;
    float        OrbitRadius    = 0.0f;
    float        RotationSpeed  = 0.0f;
    XMFLOAT3     RotationAxis   = { 0.0f, 1.0f, 0.0f };
    bool         HasRings       = false;
  };
}
