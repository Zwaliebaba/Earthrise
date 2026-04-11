#pragma once

namespace Neuron::Server
{
  // BandwidthManager — rate-limits outgoing state updates per client and
  // applies area-of-interest (AoI) filtering to reduce unnecessary traffic.
  class BandwidthManager
  {
  public:
    // Default maximum bytes per second per client.
    static constexpr int DEFAULT_BYTES_PER_SECOND = 64 * 1024; // 64 KB/s
    // Default area-of-interest radius (meters).
    static constexpr float DEFAULT_AOI_RADIUS     = 10000.0f;  // 10 km
    // Minimum interval between state snapshots for a given client (seconds).
    static constexpr double MIN_SNAPSHOT_INTERVAL  = 0.05;      // 20 Hz

    BandwidthManager() = default;
    explicit BandwidthManager(int _bytesPerSecond, float _aoiRadius) noexcept
      : m_bytesPerSecond(_bytesPerSecond)
      , m_aoiRadius(_aoiRadius)
    {
    }

    // Record that _bytes were sent to client _sessionId at _currentTime.
    void RecordSend(uint32_t _sessionId, int _bytes, double _currentTime);

    // Check whether _sessionId is within its bandwidth budget.
    [[nodiscard]] bool CanSend(uint32_t _sessionId, int _bytes, double _currentTime) const;

    // Check whether an entity at _entityPos is within the AoI of _cameraPos.
    [[nodiscard]] bool IsInAreaOfInterest(FXMVECTOR _cameraPos, FXMVECTOR _entityPos) const;

    // Check whether enough time has passed since the last snapshot to this client.
    [[nodiscard]] bool CanSendSnapshot(uint32_t _sessionId, double _currentTime) const;

    // Record that a snapshot was sent to _sessionId at _currentTime.
    void RecordSnapshot(uint32_t _sessionId, double _currentTime);

    // Remove tracking state for a disconnected client.
    void RemoveClient(uint32_t _sessionId);

    // Configuration
    void SetBytesPerSecond(int _bps) noexcept { m_bytesPerSecond = _bps; }
    void SetAoiRadius(float _radius) noexcept { m_aoiRadius = _radius; }
    [[nodiscard]] int   GetBytesPerSecond() const noexcept { return m_bytesPerSecond; }
    [[nodiscard]] float GetAoiRadius() const noexcept { return m_aoiRadius; }

  private:
    struct ClientBudget
    {
      int    BytesSentThisWindow = 0;
      double WindowStart         = 0.0;
      double LastSnapshotTime    = 0.0;
    };

    int   m_bytesPerSecond = DEFAULT_BYTES_PER_SECOND;
    float m_aoiRadius      = DEFAULT_AOI_RADIUS;

    // Per-client bandwidth tracking.
    mutable std::unordered_map<uint32_t, ClientBudget> m_budgets;

    // Internal: get or lazily create the budget for a session.
    ClientBudget& GetBudget(uint32_t _sessionId, double _currentTime) const;
  };
}
