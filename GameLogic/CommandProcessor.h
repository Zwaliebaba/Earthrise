#pragma once

#include "InputCommand.h"
#include "MovementSystem.h"

namespace GameLogic
{
  // CommandProcessor — dequeues player fleet commands and dispatches them
  // to the appropriate subsystems (movement, combat, docking, etc.).
  // Server-only.
  class CommandProcessor
  {
  public:
    CommandProcessor(SpaceObjectManager& _manager, MovementSystem& _movement);

    // Enqueue a command from a player.
    void EnqueueCommand(const InputCommand& _cmd);

    // Process all queued commands for this tick.
    void ProcessCommands();

    // Register a player's fleet ships so commands apply to them.
    void RegisterPlayerFleet(uint32_t _sessionId, std::vector<Neuron::EntityHandle> _fleet);

    // Remove a player's fleet registration.
    void UnregisterPlayer(uint32_t _sessionId);

    // Get the fleet for a session.
    [[nodiscard]] const std::vector<Neuron::EntityHandle>* GetPlayerFleet(uint32_t _sessionId) const;

  private:
    void ProcessMoveTo(const InputCommand& _cmd);
    void ProcessWarpToJumpgate(const InputCommand& _cmd);

    SpaceObjectManager& m_manager;
    MovementSystem&     m_movement;

    std::vector<InputCommand> m_pendingCommands;

    // Session ID → list of fleet ship handles.
    std::unordered_map<uint32_t, std::vector<Neuron::EntityHandle>> m_playerFleets;
  };
}
