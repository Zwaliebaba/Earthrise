#pragma once

#include "Zone.h"
#include "SessionManager.h"
#include "BandwidthManager.h"

namespace EarthRise
{
  // StateBroadcaster — serializes entities within each client's area-of-interest
  // and sends state snapshots via UDP. Respects bandwidth limits and snapshot rate.
  class StateBroadcaster
  {
  public:
    StateBroadcaster(Zone& _zone, Neuron::Server::SessionManager& _sessions,
                     Neuron::Server::BandwidthManager& _bandwidth);

    // Send state snapshots to all connected clients for the current tick.
    void BroadcastState(double _currentTime);

  private:
    // Send EntitySpawn messages for all zone entities to a newly connected client.
    void SendInitialSpawns(uint32_t _sessionId, double _currentTime);

    Zone&                            m_zone;
    Neuron::Server::SessionManager&  m_sessions;
    Neuron::Server::BandwidthManager& m_bandwidth;

    // Sessions that have received their initial entity spawn burst.
    std::unordered_set<uint32_t> m_initializedSessions;
  };
}
