#include "pch.h"
#include "CommandProcessor.h"
#include "CombatSystem.h"
#include "DockingSystem.h"

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
      case Neuron::CommandType::AttackTarget:
        ProcessAttackTarget(cmd);
        break;
      case Neuron::CommandType::Dock:
        ProcessDock(cmd);
        break;
      case Neuron::CommandType::Loot:
        ProcessLoot(cmd);
        break;
      case Neuron::CommandType::WarpToJumpgate:
        ProcessWarpToJumpgate(cmd);
        break;
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

  void CommandProcessor::ProcessAttackTarget(const InputCommand& _cmd)
  {
    if (!m_combat || !_cmd.Target.IsValid())
      return;

    const auto* fleet = GetPlayerFleet(_cmd.SessionId);
    if (!fleet)
      return;

    for (auto handle : *fleet)
    {
      if (m_manager.IsValid(handle))
        m_combat->SetAttackTarget(handle, _cmd.Target);
    }
  }

  void CommandProcessor::ProcessDock(const InputCommand& _cmd)
  {
    if (!m_docking || !_cmd.Target.IsValid())
      return;

    const auto* fleet = GetPlayerFleet(_cmd.SessionId);
    if (!fleet)
      return;

    // Move fleet toward the station, then DockingSystem will detect proximity.
    Neuron::SpaceObject* stationObj = m_manager.GetSpaceObject(_cmd.Target);
    if (!stationObj || stationObj->Category != Neuron::SpaceObjectCategory::Station)
      return;

    for (auto handle : *fleet)
    {
      if (m_manager.IsValid(handle))
      {
        m_movement.SetMoveTarget(handle, stationObj->Position);
        m_docking->CheckDockingRange(handle, _cmd.Target);
      }
    }
  }

  void CommandProcessor::ProcessLoot(const InputCommand& _cmd)
  {
    if (!_cmd.Target.IsValid())
      return;

    // Move fleet toward the crate — LootSystem will pick it up on collision.
    Neuron::SpaceObject* crateObj = m_manager.GetSpaceObject(_cmd.Target);
    if (!crateObj || crateObj->Category != Neuron::SpaceObjectCategory::Crate)
      return;

    const auto* fleet = GetPlayerFleet(_cmd.SessionId);
    if (!fleet)
      return;

    // Move only the nearest ship toward the crate.
    if (!fleet->empty())
    {
      auto handle = fleet->front();
      if (m_manager.IsValid(handle))
        m_movement.SetMoveTarget(handle, crateObj->Position);
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
