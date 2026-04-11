#include "pch.h"
#include "FleetSystem.h"

namespace GameLogic
{
  FleetSystem::FleetSystem(SpaceObjectManager& _manager, MovementSystem& _movement)
    : m_manager(_manager)
    , m_movement(_movement)
  {
  }

  Neuron::Fleet& FleetSystem::CreateFleet(uint32_t _sessionId)
  {
    auto& fleet = m_fleets[_sessionId];
    fleet.SessionId = _sessionId;
    fleet.Formation = Neuron::FormationType::None;
    fleet.ShipCount = 0;
    return fleet;
  }

  void FleetSystem::RemoveFleet(uint32_t _sessionId)
  {
    m_fleets.erase(_sessionId);
  }

  Neuron::Fleet* FleetSystem::GetFleet(uint32_t _sessionId)
  {
    auto it = m_fleets.find(_sessionId);
    return (it != m_fleets.end()) ? &it->second : nullptr;
  }

  const Neuron::Fleet* FleetSystem::GetFleet(uint32_t _sessionId) const
  {
    auto it = m_fleets.find(_sessionId);
    return (it != m_fleets.end()) ? &it->second : nullptr;
  }

  bool FleetSystem::AddShipToFleet(uint32_t _sessionId, Neuron::EntityHandle _ship)
  {
    auto* fleet = GetFleet(_sessionId);
    if (!fleet)
      return false;

    if (!m_manager.IsValid(_ship))
      return false;

    return fleet->AddShip(_ship);
  }

  bool FleetSystem::RemoveShipFromFleet(uint32_t _sessionId, Neuron::EntityHandle _ship)
  {
    auto* fleet = GetFleet(_sessionId);
    if (!fleet)
      return false;

    return fleet->RemoveShip(_ship);
  }

  void FleetSystem::RemoveShipFromAllFleets(Neuron::EntityHandle _ship)
  {
    for (auto& [id, fleet] : m_fleets)
    {
      fleet.RemoveShip(_ship);
    }
  }

  void FleetSystem::FormationMoveTo(uint32_t _sessionId, const XMFLOAT3& _targetPosition)
  {
    auto* fleet = GetFleet(_sessionId);
    if (!fleet || fleet->ShipCount == 0)
      return;

    XMVECTOR targetPos = XMLoadFloat3(&_targetPosition);

    for (uint8_t i = 0; i < fleet->ShipCount; ++i)
    {
      if (!m_manager.IsValid(fleet->Ships[i]))
        continue;

      XMFLOAT3 offset = Neuron::Fleet::GetFormationOffset(fleet->Formation, i, fleet->ShipCount);
      XMVECTOR offsetV = XMLoadFloat3(&offset);
      XMVECTOR shipTarget = XMVectorAdd(targetPos, offsetV);

      XMFLOAT3 shipTargetF;
      XMStoreFloat3(&shipTargetF, shipTarget);
      m_movement.SetMoveTarget(fleet->Ships[i], shipTargetF);
    }
  }

  void FleetSystem::SetFormation(uint32_t _sessionId, Neuron::FormationType _type)
  {
    auto* fleet = GetFleet(_sessionId);
    if (fleet)
      fleet->Formation = _type;
  }

  std::vector<Neuron::EntityHandle> FleetSystem::GetFleetShips(uint32_t _sessionId) const
  {
    std::vector<Neuron::EntityHandle> result;
    auto* fleet = GetFleet(_sessionId);
    if (fleet)
    {
      result.reserve(fleet->ShipCount);
      for (uint8_t i = 0; i < fleet->ShipCount; ++i)
        result.push_back(fleet->Ships[i]);
    }
    return result;
  }
}
