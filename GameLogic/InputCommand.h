#pragma once

#include "Messages.h"

namespace GameLogic
{
  // InputCommand — server-side representation of a player fleet command.
  struct InputCommand
  {
    uint32_t             SessionId = 0;
    Neuron::CommandType  Type      = Neuron::CommandType::MoveTo;
    Neuron::EntityHandle Target;
    XMFLOAT3             TargetPosition = {};
  };
}
