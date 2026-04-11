#include "pch.h"
#include "Prediction.h"

using namespace Neuron;

void Prediction::ApplyLocalCommand(EntityHandle _handle,
  const XMFLOAT3& _currentPos, const XMFLOAT3& _target)
{
  PredictionEntry entry;
  entry.Handle            = _handle;
  entry.CommandTarget     = _target;
  entry.HasPendingCommand = true;
  entry.PredictedPosition = _currentPos;

  m_entries[_handle.m_id] = entry;
}

void Prediction::Update(float _deltaT, float _moveSpeed)
{
  for (auto& [id, entry] : m_entries)
  {
    if (!entry.HasPendingCommand)
      continue;

    XMVECTOR pos = XMLoadFloat3(&entry.PredictedPosition);
    XMVECTOR target = XMLoadFloat3(&entry.CommandTarget);
    XMVECTOR toTarget = XMVectorSubtract(target, pos);
    float dist = XMVectorGetX(XMVector3Length(toTarget));

    if (dist < 1.0f)
    {
      // Arrived
      entry.PredictedPosition = entry.CommandTarget;
      entry.HasPendingCommand = false;
      continue;
    }

    float step = _moveSpeed * _deltaT;
    if (step > dist) step = dist;

    XMVECTOR dir = XMVector3Normalize(toTarget);
    XMVECTOR newPos = XMVectorAdd(pos, XMVectorScale(dir, step));
    XMStoreFloat3(&entry.PredictedPosition, newPos);
  }
}

void Prediction::Reconcile(EntityHandle _handle,
  const XMFLOAT3& _serverPos, float _snapThreshold)
{
  auto it = m_entries.find(_handle.m_id);
  if (it == m_entries.end())
    return;

  auto& entry = it->second;

  XMVECTOR predicted = XMLoadFloat3(&entry.PredictedPosition);
  XMVECTOR server = XMLoadFloat3(&_serverPos);
  float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(predicted, server)));

  if (dist > _snapThreshold)
  {
    // Snap to server position
    entry.PredictedPosition = _serverPos;
  }
  else
  {
    // Blend toward server position (smooth reconciliation)
    XMVECTOR blended = XMVectorLerp(predicted, server, 0.3f);
    XMStoreFloat3(&entry.PredictedPosition, blended);
  }
}

const XMFLOAT3* Prediction::GetPredictedPosition(EntityHandle _handle) const
{
  auto it = m_entries.find(_handle.m_id);
  if (it != m_entries.end() && it->second.HasPendingCommand)
    return &it->second.PredictedPosition;
  return nullptr;
}

void Prediction::Remove(EntityHandle _handle)
{
  m_entries.erase(_handle.m_id);
}

void Prediction::Clear()
{
  m_entries.clear();
}

bool Prediction::HasPrediction(EntityHandle _handle) const
{
  auto it = m_entries.find(_handle.m_id);
  return it != m_entries.end() && it->second.HasPendingCommand;
}
