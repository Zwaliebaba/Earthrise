#include "pch.h"
#include "StateBroadcaster.h"
#include "DataWriter.h"
#include "GameTypes/ObjectDefs.h"

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

    // Spawn a player ship near the origin and notify the client.
    Neuron::EntityHandle flagship = m_zone.GetEntityManager().CreateEntity(
      Neuron::SpaceObjectCategory::Ship,
      XMFLOAT3{ 0.0f, 0.0f, 20.0f },
      Neuron::HashMeshName("Shuttle"));

    if (flagship.IsValid())
    {
      if (auto* obj = m_zone.GetEntityManager().GetSpaceObject(flagship))
      {
        obj->BoundingRadius = 1.5f;
        obj->Color = { 0.2f, 0.6f, 1.0f, 1.0f };
      }
      if (auto* ship = m_zone.GetEntityManager().GetShipData(flagship))
      {
        ship->ShieldHP    = 100.0f;
        ship->ShieldMaxHP = 100.0f;
        ship->ArmorHP     = 80.0f;
        ship->ArmorMaxHP  = 100.0f;
        ship->HullHP      = 150.0f;
        ship->HullMaxHP   = 150.0f;
        ship->Energy      = 75.0f;
        ship->EnergyMax   = 100.0f;
        ship->MaxSpeed    = 50.0f;
        ship->IsPlayer    = true;
        ship->FactionId   = 1;
      }

      // Send the spawn for the player ship
      {
        Neuron::EntitySpawnMsg spawn;
        spawn.Handle   = flagship;
        spawn.Category = Neuron::SpaceObjectCategory::Ship;
        spawn.MeshHash = Neuron::HashMeshName("Shuttle");
        spawn.Position = { 0.0f, 0.0f, 20.0f };
        spawn.Orientation = { 0, 0, 0, 1 };
        spawn.Color    = { 0.2f, 0.6f, 1.0f, 1.0f };

        Neuron::DataWriter writer;
        spawn.Write(writer);
        m_sessions.SendReliable(_sessionId, Neuron::MessageId::EntitySpawn,
                                writer.Data(), writer.Size(), _currentTime);
        ++count;
      }

      // Tell the client which entity is their flagship
      {
        Neuron::PlayerInfoMsg info;
        info.FlagshipHandle = flagship;

        Neuron::DataWriter writer;
        info.Write(writer);
        m_sessions.SendReliable(_sessionId, Neuron::MessageId::PlayerInfo,
                                writer.Data(), writer.Size(), _currentTime);
      }

      Neuron::Server::ServerLog("StateBroadcaster: spawned player ship (handle {}) for session {}\n",
                                flagship.m_id, _sessionId);
    }

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

  void StateBroadcaster::BroadcastShipStatus(double _currentTime)
  {
    // ShipStatusEntry = 4 + 10*4 + 2 = 46 bytes per ship.
    // Header = 2 (count). Max entries: (1400 - 8 - 2) / 46 ≈ 30.
    static constexpr int MAX_SHIPS_PER_PACKET = 30;

    for (uint32_t sid = 1; sid <= Neuron::Server::SessionManager::MAX_CLIENTS; ++sid)
    {
      Neuron::Server::ClientSession* session = m_sessions.FindSession(sid);
      if (!session || session->GetState() != Neuron::Server::ClientSession::State::Connected)
        continue;

      if (!m_initializedSessions.contains(sid))
        continue;

      Neuron::ShipStatusMsg statusMsg;

      auto& mgr = m_zone.GetEntityManager();
      mgr.ForEachActive([&](Neuron::SpaceObject& _obj)
      {
        if (_obj.Category != Neuron::SpaceObjectCategory::Ship) return;

        const auto* ship = mgr.GetShipData(_obj.Handle);
        if (!ship) return;

        Neuron::ShipStatusEntry entry;
        entry.HandleId    = _obj.Handle.m_id;
        entry.ShieldHP    = ship->ShieldHP;
        entry.ShieldMaxHP = ship->ShieldMaxHP;
        entry.ArmorHP     = ship->ArmorHP;
        entry.ArmorMaxHP  = ship->ArmorMaxHP;
        entry.HullHP      = ship->HullHP;
        entry.HullMaxHP   = ship->HullMaxHP;
        entry.Energy      = ship->Energy;
        entry.EnergyMax   = ship->EnergyMax;

        // Compute speed from velocity
        XMVECTOR vel = XMLoadFloat3(&_obj.Velocity);
        entry.Speed    = XMVectorGetX(XMVector3Length(vel));
        entry.MaxSpeed = ship->MaxSpeed;
        entry.FactionId = ship->FactionId;

        statusMsg.Ships.push_back(entry);

        if (static_cast<int>(statusMsg.Ships.size()) >= MAX_SHIPS_PER_PACKET)
        {
          Neuron::DataWriter writer;
          statusMsg.Write(writer);
          if (m_bandwidth.CanSend(sid, writer.Size(), _currentTime))
          {
            m_sessions.SendTo(sid, Neuron::MessageId::ShipStatus, 0,
                              writer.Data(), writer.Size());
            m_bandwidth.RecordSend(sid, writer.Size(), _currentTime);
          }
          statusMsg.Ships.clear();
        }
      });

      if (!statusMsg.Ships.empty())
      {
        Neuron::DataWriter writer;
        statusMsg.Write(writer);
        if (m_bandwidth.CanSend(sid, writer.Size(), _currentTime))
        {
          m_sessions.SendTo(sid, Neuron::MessageId::ShipStatus, 0,
                            writer.Data(), writer.Size());
          m_bandwidth.RecordSend(sid, writer.Size(), _currentTime);
        }
      }
    }
  }
}
