#include "pch.h"
#include "CommandProcessor.h"

namespace GameLogic
{
  CommandProcessor::CommandProcessor(SpaceObjectManager& _manager, MovementSystem& _movement)
    : m_manager(_manager)
    , m_movement(_movement)
  {
  }

  void CommandProcessor::EnqueueCommand(const InputCommand& _cmd)
  {
    m_pendingCommands.push_back(_cmd);
  }

  void CommandProcessor::ProcessCommands()
  {
    for (const auto& cmd : m_pendingCommands)
    {
      switch (cmd.Type)
      {
      case Neuron::CommandType::MoveTo:
        ProcessMoveTo(cmd);
        break;
      case Neuron::CommandType::WarpToJumpgate:
        ProcessWarpToJumpgate(cmd);
        break;
      // Other command types (AttackTarget, Dock, Loot, UseAbility) are
      // dispatched here as the corresponding systems are implemented.
      default:
        break;
      }
    }
    m_pendingCommands.clear();
  }

  void CommandProcessor::RegisterPlayerFleet(uint32_t _sessionId, std::vector<Neuron::EntityHandle> _fleet)
  {
    m_playerFleets[_sessionId] = std::move(_fleet);
  }

  void CommandProcessor::UnregisterPlayer(uint32_t _sessionId)
  {
    m_playerFleets.erase(_sessionId);
  }

  const std::vector<Neuron::EntityHandle>* CommandProcessor::GetPlayerFleet(uint32_t _sessionId) const
  {
    auto it = m_playerFleets.find(_sessionId);
    if (it == m_playerFleets.end())
      return nullptr;
    return &it->second;
  }

  void CommandProcessor::ProcessMoveTo(const InputCommand& _cmd)
  {
    const auto* fleet = GetPlayerFleet(_cmd.SessionId);
    if (!fleet)
      return;

    for (auto handle : *fleet)
    {
      if (m_manager.IsValid(handle))
        m_movement.SetMoveTarget(handle, _cmd.TargetPosition);
    }
  }

  void CommandProcessor::ProcessWarpToJumpgate(const InputCommand& _cmd)
  {
    // Jumpgate warp is handled by JumpgateSystem during tick.
    // CommandProcessor sets a move target to the jumpgate position,
    // and JumpgateSystem detects proximity to trigger the warp.
    if (!_cmd.Target.IsValid())
      return;

    Neuron::SpaceObject* gateObj = m_manager.GetSpaceObject(_cmd.Target);
    if (!gateObj || gateObj->Category != Neuron::SpaceObjectCategory::Jumpgate)
      return;

    // Move fleet to the jumpgate position
    const auto* fleet = GetPlayerFleet(_cmd.SessionId);
    if (!fleet)
      return;

    for (auto handle : *fleet)
    {
      if (m_manager.IsValid(handle))
        m_movement.SetMoveTarget(handle, gateObj->Position);
    }
  }
}
