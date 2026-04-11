#include "pch.h"
#include "BandwidthManager.h"

namespace Neuron::Server
{
  void BandwidthManager::RecordSend(uint32_t _sessionId, int _bytes, double _currentTime)
  {
    auto& budget = GetBudget(_sessionId, _currentTime);
    budget.BytesSentThisWindow += _bytes;
  }

  bool BandwidthManager::CanSend(uint32_t _sessionId, int _bytes, double _currentTime) const
  {
    const auto& budget = GetBudget(_sessionId, _currentTime);
    return (budget.BytesSentThisWindow + _bytes) <= m_bytesPerSecond;
  }

  bool BandwidthManager::IsInAreaOfInterest(FXMVECTOR _cameraPos, FXMVECTOR _entityPos) const
  {
    XMVECTOR diff = XMVectorSubtract(_entityPos, _cameraPos);
    float distSq = XMVectorGetX(XMVector3LengthSq(diff));
    return distSq <= (m_aoiRadius * m_aoiRadius);
  }

  bool BandwidthManager::CanSendSnapshot(uint32_t _sessionId, double _currentTime) const
  {
    auto it = m_budgets.find(_sessionId);
    if (it == m_budgets.end())
      return true;
    return (_currentTime - it->second.LastSnapshotTime) >= MIN_SNAPSHOT_INTERVAL;
  }

  void BandwidthManager::RecordSnapshot(uint32_t _sessionId, double _currentTime)
  {
    auto& budget = GetBudget(_sessionId, _currentTime);
    budget.LastSnapshotTime = _currentTime;
  }

  void BandwidthManager::RemoveClient(uint32_t _sessionId)
  {
    m_budgets.erase(_sessionId);
  }

  BandwidthManager::ClientBudget& BandwidthManager::GetBudget(
    uint32_t _sessionId, double _currentTime) const
  {
    auto& budget = m_budgets[_sessionId];

    // Reset window every second.
    if (_currentTime - budget.WindowStart >= 1.0)
    {
      budget.BytesSentThisWindow = 0;
      budget.WindowStart         = _currentTime;
    }

    return budget;
  }
}
