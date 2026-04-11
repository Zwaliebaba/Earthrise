#include "pch.h"
#include "NpcAI.h"

namespace GameLogic
{
  NpcAI::NpcAI(SpaceObjectManager& _manager, MovementSystem& _movement, CombatSystem& _combat)
    : m_manager(_manager)
    , m_movement(_movement)
    , m_combat(_combat)
  {
    m_npcStates.resize(MAX_ENTITIES);
    m_isNpc.resize(MAX_ENTITIES, false);
  }

  void NpcAI::RegisterNpc(Neuron::EntityHandle _handle, uint16_t _factionId,
                           const XMFLOAT3* _waypoints, uint8_t _waypointCount,
                           float _aggroRadius, float _attackRange)
  {
    if (!m_manager.IsValid(_handle))
      return;

    uint32_t idx = _handle.Index() - 1;
    m_isNpc[idx] = true;

    auto& state = m_npcStates[idx];
    state = NpcState{};
    state.FactionId = _factionId;
    state.AggroRadius = _aggroRadius;
    state.LeashRadius = _aggroRadius * 2.0f;
    state.AttackRange = _attackRange;

    uint8_t count = std::min(_waypointCount, static_cast<uint8_t>(4));
    state.WaypointCount = count;
    for (uint8_t i = 0; i < count; ++i)
      state.Waypoints[i] = _waypoints[i];

    state.State = (count > 0) ? AiState::Patrol : AiState::Idle;
  }

  void NpcAI::UnregisterNpc(Neuron::EntityHandle _handle)
  {
    if (!_handle.IsValid())
      return;

    uint32_t idx = _handle.Index() - 1;
    if (idx < m_isNpc.size())
    {
      m_isNpc[idx] = false;
      m_npcStates[idx] = NpcState{};
    }
  }

  const NpcState* NpcAI::GetNpcState(Neuron::EntityHandle _handle) const
  {
    if (!_handle.IsValid())
      return nullptr;

    uint32_t idx = _handle.Index() - 1;
    if (idx >= m_isNpc.size() || !m_isNpc[idx])
      return nullptr;

    return &m_npcStates[idx];
  }

  void NpcAI::Update(float _deltaTime)
  {
    m_manager.ForEachCategory(Neuron::SpaceObjectCategory::Ship, [&](Neuron::SpaceObject& _obj)
    {
      uint32_t idx = _obj.Handle.Index() - 1;
      if (idx >= m_isNpc.size() || !m_isNpc[idx])
        return;

      auto& state = m_npcStates[idx];

      switch (state.State)
      {
      case AiState::Idle:    UpdateIdle(_obj, state, _deltaTime);   break;
      case AiState::Patrol:  UpdatePatrol(_obj, state, _deltaTime); break;
      case AiState::Aggro:   UpdateAggro(_obj, state, _deltaTime);  break;
      case AiState::Chase:   UpdateChase(_obj, state, _deltaTime);  break;
      case AiState::Attack:  UpdateAttack(_obj, state, _deltaTime); break;
      case AiState::Return:  UpdateReturn(_obj, state, _deltaTime); break;
      default: break;
      }
    });
  }

  void NpcAI::UpdateIdle(Neuron::SpaceObject& _obj, NpcState& _state, float _dt)
  {
    // Periodically scan for hostiles.
    _state.ScanTimer -= _dt;
    if (_state.ScanTimer <= 0.0f)
    {
      _state.ScanTimer = NpcState::SCAN_INTERVAL;
      auto hostile = ScanForHostile(_obj, _state);
      if (hostile.IsValid())
      {
        _state.Target = hostile;
        _state.State = AiState::Aggro;
      }
    }
  }

  void NpcAI::UpdatePatrol(Neuron::SpaceObject& _obj, NpcState& _state, float _dt)
  {
    // Scan for hostiles while patrolling.
    _state.ScanTimer -= _dt;
    if (_state.ScanTimer <= 0.0f)
    {
      _state.ScanTimer = NpcState::SCAN_INTERVAL;
      auto hostile = ScanForHostile(_obj, _state);
      if (hostile.IsValid())
      {
        _state.Target = hostile;
        _state.State = AiState::Aggro;
        return;
      }
    }

    // Move toward current waypoint.
    if (_state.WaypointCount == 0)
    {
      _state.State = AiState::Idle;
      return;
    }

    XMVECTOR pos = XMLoadFloat3(&_obj.Position);
    XMVECTOR wp = XMLoadFloat3(&_state.Waypoints[_state.CurrentWaypoint]);
    float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(wp, pos)));

    if (dist < 5.0f) // Arrived at waypoint
    {
      _state.CurrentWaypoint = (_state.CurrentWaypoint + 1) % _state.WaypointCount;
    }

    m_movement.SetMoveTarget(_obj.Handle, _state.Waypoints[_state.CurrentWaypoint]);
  }

  void NpcAI::UpdateAggro(Neuron::SpaceObject& /*_obj*/, NpcState& _state, float /*_dt*/)
  {
    // Brief transition state — immediately move to chase.
    _state.State = AiState::Chase;
  }

  void NpcAI::UpdateChase(Neuron::SpaceObject& _obj, NpcState& _state, float /*_dt*/)
  {
    // Validate target.
    Neuron::SpaceObject* targetObj = m_manager.GetSpaceObject(_state.Target);
    if (!targetObj || !targetObj->Active)
    {
      _state.Target = Neuron::EntityHandle::Invalid();
      _state.State = (_state.WaypointCount > 0) ? AiState::Return : AiState::Idle;
      m_combat.ClearAttackTarget(_obj.Handle);
      return;
    }

    XMVECTOR pos = XMLoadFloat3(&_obj.Position);
    XMVECTOR targetPos = XMLoadFloat3(&targetObj->Position);
    float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(targetPos, pos)));

    // Check leash distance — give up if too far.
    if (dist > _state.LeashRadius)
    {
      _state.Target = Neuron::EntityHandle::Invalid();
      _state.State = (_state.WaypointCount > 0) ? AiState::Return : AiState::Idle;
      m_combat.ClearAttackTarget(_obj.Handle);
      m_movement.ClearMoveTarget(_obj.Handle);
      return;
    }

    // Within attack range → transition to Attack.
    if (dist <= _state.AttackRange)
    {
      _state.State = AiState::Attack;
      m_combat.SetAttackTarget(_obj.Handle, _state.Target);
      return;
    }

    // Chase: move toward target.
    m_movement.SetMoveTarget(_obj.Handle, targetObj->Position);
  }

  void NpcAI::UpdateAttack(Neuron::SpaceObject& _obj, NpcState& _state, float /*_dt*/)
  {
    // Validate target.
    Neuron::SpaceObject* targetObj = m_manager.GetSpaceObject(_state.Target);
    if (!targetObj || !targetObj->Active)
    {
      _state.Target = Neuron::EntityHandle::Invalid();
      _state.State = (_state.WaypointCount > 0) ? AiState::Return : AiState::Idle;
      m_combat.ClearAttackTarget(_obj.Handle);
      m_movement.ClearMoveTarget(_obj.Handle);
      return;
    }

    XMVECTOR pos = XMLoadFloat3(&_obj.Position);
    XMVECTOR targetPos = XMLoadFloat3(&targetObj->Position);
    float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(targetPos, pos)));

    // Target out of attack range → chase again.
    if (dist > _state.AttackRange * 1.2f) // Hysteresis to avoid flickering
    {
      _state.State = AiState::Chase;
      m_combat.ClearAttackTarget(_obj.Handle);
      return;
    }

    // CombatSystem handles the actual firing. Just keep the attack target set.
    m_combat.SetAttackTarget(_obj.Handle, _state.Target);
  }

  void NpcAI::UpdateReturn(Neuron::SpaceObject& _obj, NpcState& _state, float _dt)
  {
    // Scan for new hostiles while returning.
    _state.ScanTimer -= _dt;
    if (_state.ScanTimer <= 0.0f)
    {
      _state.ScanTimer = NpcState::SCAN_INTERVAL;
      auto hostile = ScanForHostile(_obj, _state);
      if (hostile.IsValid())
      {
        _state.Target = hostile;
        _state.State = AiState::Aggro;
        return;
      }
    }

    // Move toward current waypoint to resume patrol.
    if (_state.WaypointCount == 0)
    {
      _state.State = AiState::Idle;
      return;
    }

    XMVECTOR pos = XMLoadFloat3(&_obj.Position);
    XMVECTOR wp = XMLoadFloat3(&_state.Waypoints[_state.CurrentWaypoint]);
    float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(wp, pos)));

    if (dist < 5.0f)
    {
      _state.State = AiState::Patrol;
      return;
    }

    m_movement.SetMoveTarget(_obj.Handle, _state.Waypoints[_state.CurrentWaypoint]);
  }

  Neuron::EntityHandle NpcAI::ScanForHostile(const Neuron::SpaceObject& _obj, const NpcState& _state)
  {
    Neuron::EntityHandle closest = Neuron::EntityHandle::Invalid();
    float closestDistSq = _state.AggroRadius * _state.AggroRadius;

    XMVECTOR myPos = XMLoadFloat3(&_obj.Position);

    m_manager.ForEachCategory(Neuron::SpaceObjectCategory::Ship, [&](Neuron::SpaceObject& _other)
    {
      if (_other.Handle == _obj.Handle)
        return; // Skip self

      // Check faction — different faction = hostile.
      Neuron::ShipData* otherShip = m_manager.GetShipData(_other.Handle);
      if (!otherShip || otherShip->FactionId == _state.FactionId)
        return; // Same faction = friendly

      XMVECTOR otherPos = XMLoadFloat3(&_other.Position);
      float distSq = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(otherPos, myPos)));

      if (distSq < closestDistSq)
      {
        closestDistSq = distSq;
        closest = _other.Handle;
      }
    });

    return closest;
  }
}
