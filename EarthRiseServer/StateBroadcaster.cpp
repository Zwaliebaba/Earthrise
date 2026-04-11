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

  void StateBroadcaster::SendInitialSpawns(uint32_t _sessionId, double _currentTime)
  {
    // Send EntitySpawn (reliable) for every active entity in the zone.
    uint32_t count = 0;

    m_zone.GetEntityManager().ForEachActive([&](Neuron::SpaceObject& _obj)
    {
      Neuron::EntitySpawnMsg spawn;
      spawn.Handle      = _obj.Handle;
      spawn.Category    = _obj.Category;
      spawn.MeshHash    = _obj.MeshNameHash;
      spawn.Position    = _obj.Position;
      spawn.Orientation = _obj.Orientation;
      spawn.Color       = _obj.Color;

      Neuron::DataWriter writer;
      spawn.Write(writer);

      m_sessions.SendReliable(_sessionId, Neuron::MessageId::EntitySpawn,
                              writer.Data(), writer.Size(), _currentTime);
      ++count;
    });

    Neuron::Server::ServerLog("StateBroadcaster: sent {} entity spawns to session {}\n",
                              count, _sessionId);
  }

  void StateBroadcaster::BroadcastState(double _currentTime)
  {
    // Entities per datagram: each EntityState = 4 + 12 + 16 + 12 = 44 bytes
    // SnapshotMsg header = 4 (tick) + 2 (count) = 6 bytes
    // PacketHeader = 8 bytes
    // Max entities per packet: (1400 - 8 - 6) / 44 ≈ 31
    static constexpr int MAX_ENTITIES_PER_PACKET = 31;

    uint32_t tick = m_zone.TickCount();

    for (uint32_t sid = 1; sid <= Neuron::Server::SessionManager::MAX_CLIENTS; ++sid)
    {
      Neuron::Server::ClientSession* session = m_sessions.FindSession(sid);
      if (!session || session->GetState() != Neuron::Server::ClientSession::State::Connected)
      {
        // Clean up tracking for disconnected sessions.
        m_initializedSessions.erase(sid);
        continue;
      }

      // Send initial entity spawns to newly connected clients.
      if (!m_initializedSessions.contains(sid))
      {
        SendInitialSpawns(sid, _currentTime);
        m_initializedSessions.insert(sid);
      }

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
