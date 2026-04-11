#include "pch.h"
#include "ClientWorldState.h"

using namespace Neuron;

void ClientWorldState::ApplySpawn(const EntitySpawnMsg& _msg)
{
  ClientEntity entity;
  entity.Handle      = _msg.Handle;
  entity.Category    = _msg.Category;
  entity.MeshHash    = _msg.MeshHash;
  entity.Active      = true;

  entity.Position         = _msg.Position;
  entity.Orientation      = _msg.Orientation;
  entity.Color            = _msg.Color;
  entity.Velocity         = {};

  // Initialize interpolation state to current position (no lerp jitter on first frame)
  entity.PrevPosition      = _msg.Position;
  entity.PrevOrientation   = _msg.Orientation;
  entity.TargetPosition    = _msg.Position;
  entity.TargetOrientation = _msg.Orientation;

  // Resolve mesh key from hash
  auto it = m_meshHashToKey.find(_msg.MeshHash);
  if (it != m_meshHashToKey.end())
    entity.MeshKey = it->second;

  m_entities[_msg.Handle.m_id] = std::move(entity);
  m_activeCount = 0;
  for (const auto& [id, e] : m_entities)
    if (e.Active) ++m_activeCount;
}

void ClientWorldState::ApplyDespawn(const EntityDespawnMsg& _msg)
{
  auto it = m_entities.find(_msg.Handle.m_id);
  if (it != m_entities.end())
  {
    it->second.Active = false;
    m_entities.erase(it);
    if (m_activeCount > 0) --m_activeCount;
  }
}

void ClientWorldState::ApplySnapshot(const StateSnapshotMsg& _msg)
{
  m_lastServerTick = _msg.ServerTick;

  for (const auto& state : _msg.Entities)
  {
    auto it = m_entities.find(state.HandleId);
    if (it == m_entities.end())
      continue;

    auto& entity = it->second;

    // Move current target to prev (for interpolation)
    entity.PrevPosition    = entity.TargetPosition;
    entity.PrevOrientation = entity.TargetOrientation;

    // Set new target from server snapshot
    entity.TargetPosition    = state.Position;
    entity.TargetOrientation = state.Orientation;
    entity.Velocity          = state.Velocity;
  }

  // Reset interpolation alpha on new snapshot
  m_interpAlpha = 0.0f;
}

void ClientWorldState::Update(float _deltaT)
{
  // Advance interpolation alpha
  m_interpAlpha += _deltaT / SNAPSHOT_INTERVAL;
  if (m_interpAlpha > 1.0f)
    m_interpAlpha = 1.0f;

  const float t = m_interpAlpha;

  for (auto& [id, entity] : m_entities)
  {
    if (!entity.Active)
      continue;

    // Lerp position
    XMVECTOR prev = XMLoadFloat3(&entity.PrevPosition);
    XMVECTOR target = XMLoadFloat3(&entity.TargetPosition);
    XMVECTOR interpPos = XMVectorLerp(prev, target, t);

    // If alpha >= 1.0, extrapolate using velocity (dead reckoning)
    if (m_interpAlpha >= 1.0f)
    {
      XMVECTOR vel = XMLoadFloat3(&entity.Velocity);
      float overshoot = (m_interpAlpha - 1.0f) * SNAPSHOT_INTERVAL;
      interpPos = XMVectorAdd(target, XMVectorScale(vel, overshoot));
    }

    XMStoreFloat3(&entity.Position, interpPos);

    // Slerp orientation
    XMVECTOR prevOri = XMLoadFloat4(&entity.PrevOrientation);
    XMVECTOR targetOri = XMLoadFloat4(&entity.TargetOrientation);
    XMVECTOR interpOri = XMQuaternionSlerp(prevOri, targetOri, t);
    XMStoreFloat4(&entity.Orientation, interpOri);
  }
}

const ClientEntity* ClientWorldState::GetEntity(EntityHandle _handle) const
{
  auto it = m_entities.find(_handle.m_id);
  if (it != m_entities.end() && it->second.Active)
    return &it->second;
  return nullptr;
}

ClientEntity* ClientWorldState::GetEntity(EntityHandle _handle)
{
  auto it = m_entities.find(_handle.m_id);
  if (it != m_entities.end() && it->second.Active)
    return &it->second;
  return nullptr;
}

void ClientWorldState::Clear()
{
  m_entities.clear();
  m_activeCount = 0;
  m_lastServerTick = 0;
  m_interpAlpha = 0.0f;
}

void ClientWorldState::RegisterMeshName(uint32_t _hash, std::string_view _key)
{
  m_meshHashToKey[_hash] = std::string(_key);
}
