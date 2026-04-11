#include "pch.h"
#include "StateBroadcaster.h"
#include "DataWriter.h"

namespace EarthRise
{
  StateBroadcaster::StateBroadcaster(Zone& _zone,
                                     Neuron::Server::SessionManager& _sessions,
                                     Neuron::Server::BandwidthManager& _bandwidth)
    : m_zone(_zone)
    , m_sessions(_sessions)
    , m_bandwidth(_bandwidth)
  {
  }

  void StateBroadcaster::BroadcastState(double _currentTime)
  {
    // For each connected session, build a state snapshot of nearby entities.
    // This iterates sessions, so we need access to them. SessionManager doesn't
    // expose an iterator directly, so we'll iterate known session IDs.
    // For now, use a simple approach: iterate all active entities into one snapshot
    // per client, filtered by AoI.

    // Entities per datagram: each EntityState = 4 + 12 + 16 + 12 = 44 bytes
    // SnapshotMsg header = 4 (tick) + 2 (count) = 6 bytes
    // PacketHeader = 8 bytes
    // Max entities per packet: (1400 - 8 - 6) / 44 ≈ 31
    static constexpr int MAX_ENTITIES_PER_PACKET = 31;

    uint32_t tick = m_zone.TickCount();

    // We need to iterate over sessions. Use the session manager's ForEach if available.
    // Since SessionManager doesn't expose ForEach, we'll track sessions externally
    // via the session count. For now, broadcast to all session IDs 1..max.
    // This is a simplified approach — production would use a session list.

    for (uint32_t sid = 1; sid <= Neuron::Server::SessionManager::MAX_CLIENTS; ++sid)
    {
      Neuron::Server::ClientSession* session = m_sessions.FindSession(sid);
      if (!session || session->GetState() != Neuron::Server::ClientSession::State::Connected)
        continue;

      if (!m_bandwidth.CanSendSnapshot(sid, _currentTime))
        continue;

      // Use the session's position (camera position) for AoI filtering.
      // For now, use origin — until we track per-player camera positions.
      XMVECTOR cameraPos = XMVectorZero();

      // Collect entities in AoI
      Neuron::StateSnapshotMsg snapshot;
      snapshot.ServerTick = tick;

      m_zone.GetEntityManager().ForEachActive([&](Neuron::SpaceObject& _obj)
      {
        XMVECTOR entityPos = XMLoadFloat3(&_obj.Position);
        if (!m_bandwidth.IsInAreaOfInterest(cameraPos, entityPos))
          return;

        Neuron::EntityState state;
        state.HandleId    = _obj.Handle.m_id;
        state.Position    = _obj.Position;
        state.Orientation = _obj.Orientation;
        state.Velocity    = _obj.Velocity;
        snapshot.Entities.push_back(state);

        // Flush when we hit the per-packet limit
        if (static_cast<int>(snapshot.Entities.size()) >= MAX_ENTITIES_PER_PACKET)
        {
          Neuron::DataWriter writer;
          snapshot.Write(writer);

          if (m_bandwidth.CanSend(sid, writer.Size(), _currentTime))
          {
            m_sessions.SendTo(sid, Neuron::MessageId::StateSnapshot, 0,
                              writer.Data(), writer.Size());
            m_bandwidth.RecordSend(sid, writer.Size(), _currentTime);
          }

          snapshot.Entities.clear();
        }
      });

      // Send remaining entities
      if (!snapshot.Entities.empty())
      {
        Neuron::DataWriter writer;
        snapshot.Write(writer);

        if (m_bandwidth.CanSend(sid, writer.Size(), _currentTime))
        {
          m_sessions.SendTo(sid, Neuron::MessageId::StateSnapshot, 0,
                            writer.Data(), writer.Size());
          m_bandwidth.RecordSend(sid, writer.Size(), _currentTime);
        }
      }

      m_bandwidth.RecordSnapshot(sid, _currentTime);
    }
  }
}
