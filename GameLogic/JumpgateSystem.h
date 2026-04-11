#pragma once

#include "SpaceObjectManager.h"

namespace GameLogic
{
  // JumpgateSystem — handles jumpgate fast-travel teleportation.
  // When a ship is within activation radius of a jumpgate, it can be warped
  // to the gate's destination position.
  class JumpgateSystem
  {
  public:
    explicit JumpgateSystem(SpaceObjectManager& _manager);

    // Check all ships near jumpgates and teleport those that are within range
    // and have been flagged for warp.
    void Update();

    // Flag a fleet for jumpgate warp. The fleet will be teleported when
    // at least one ship is within the gate's activation radius.
    void RequestWarp(Neuron::EntityHandle _gateHandle,
                     const std::vector<Neuron::EntityHandle>& _fleet);

    // Pending warp request.
    struct WarpRequest
    {
      Neuron::EntityHandle GateHandle;
      std::vector<Neuron::EntityHandle> Fleet;
    };

    // Get completed warps from the last update (for notification).
    [[nodiscard]] const std::vector<WarpRequest>& CompletedWarps() const noexcept
    {
      return m_completedWarps;
    }

  private:
    SpaceObjectManager& m_manager;
    std::vector<WarpRequest> m_pendingWarps;
    std::vector<WarpRequest> m_completedWarps;
  };
}
