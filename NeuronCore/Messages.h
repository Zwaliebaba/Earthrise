#pragma once

// Messages.h — Concrete message structs for the client ↔ server protocol.
// Each message type provides Write(DataWriter&) and Read(DataReader&) for
// serialization into/from 1400-byte UDP datagrams.

namespace Neuron
{
  // ------------------------------------------------------------------
  // LoginRequest (client → server, reliable)
  // ------------------------------------------------------------------
  struct LoginRequest
  {
    std::string PlayerName;
    uint32_t    ProtocolVersion = ENGINE_VERSION;

    void Write(DataWriter& _w) const
    {
      _w.WriteString(PlayerName);
      _w.Write<uint32_t>(ProtocolVersion);
    }

    void Read(DataReader& _r)
    {
      PlayerName      = _r.ReadString();
      ProtocolVersion = _r.Read<uint32_t>();
    }
  };

  // ------------------------------------------------------------------
  // LoginResponse (server → client, reliable)
  // ------------------------------------------------------------------
  enum class LoginResult : uint8_t
  {
    Success       = 0,
    VersionMismatch,
    ServerFull,
    Rejected,
  };

  struct LoginResponse
  {
    LoginResult Result    = LoginResult::Success;
    uint32_t    SessionId = 0;    // Assigned by server on success

    void Write(DataWriter& _w) const
    {
      _w.Write<uint8_t>(static_cast<uint8_t>(Result));
      _w.Write<uint32_t>(SessionId);
    }

    void Read(DataReader& _r)
    {
      Result    = static_cast<LoginResult>(_r.Read<uint8_t>());
      SessionId = _r.Read<uint32_t>();
    }
  };

  // ------------------------------------------------------------------
  // Disconnect (either direction, reliable)
  // ------------------------------------------------------------------
  enum class DisconnectReason : uint8_t
  {
    Graceful = 0,
    Timeout,
    Kicked,
  };

  struct DisconnectMsg
  {
    DisconnectReason Reason = DisconnectReason::Graceful;

    void Write(DataWriter& _w) const
    {
      _w.Write<uint8_t>(static_cast<uint8_t>(Reason));
    }

    void Read(DataReader& _r)
    {
      Reason = static_cast<DisconnectReason>(_r.Read<uint8_t>());
    }
  };

  // ------------------------------------------------------------------
  // Heartbeat (either direction, unreliable)
  // ------------------------------------------------------------------
  struct HeartbeatMsg
  {
    uint32_t ClientTime = 0;  // ms since connection start

    void Write(DataWriter& _w) const
    {
      _w.Write<uint32_t>(ClientTime);
    }

    void Read(DataReader& _r)
    {
      ClientTime = _r.Read<uint32_t>();
    }
  };

  // ------------------------------------------------------------------
  // EntitySpawn (server → client, reliable)
  // ------------------------------------------------------------------
  struct EntitySpawnMsg
  {
    EntityHandle        Handle;
    SpaceObjectCategory Category  = SpaceObjectCategory::SpaceObject;
    uint32_t            MeshHash  = 0;
    XMFLOAT3            Position  = {};
    XMFLOAT4            Orientation = { 0, 0, 0, 1 };
    XMFLOAT4            Color     = { 1, 1, 1, 1 };

    void Write(DataWriter& _w) const
    {
      _w.Write<uint32_t>(Handle.m_id);
      _w.Write<uint8_t>(static_cast<uint8_t>(Category));
      _w.Write<uint32_t>(MeshHash);
      _w.Write<XMFLOAT3>(Position);
      _w.Write<XMFLOAT4>(Orientation);
      _w.Write<XMFLOAT4>(Color);
    }

    void Read(DataReader& _r)
    {
      Handle.m_id = _r.Read<uint32_t>();
      Category    = static_cast<SpaceObjectCategory>(_r.Read<uint8_t>());
      MeshHash    = _r.Read<uint32_t>();
      Position    = _r.Read<XMFLOAT3>();
      Orientation = _r.Read<XMFLOAT4>();
      Color       = _r.Read<XMFLOAT4>();
    }
  };

  // ------------------------------------------------------------------
  // EntityDespawn (server → client, reliable)
  // ------------------------------------------------------------------
  struct EntityDespawnMsg
  {
    EntityHandle Handle;

    void Write(DataWriter& _w) const
    {
      _w.Write<uint32_t>(Handle.m_id);
    }

    void Read(DataReader& _r)
    {
      Handle.m_id = _r.Read<uint32_t>();
    }
  };

  // ------------------------------------------------------------------
  // StateSnapshot (server → client, unreliable)
  // Each datagram carries a batch of entity state entries.
  // ------------------------------------------------------------------
  struct EntityState
  {
    uint32_t HandleId   = 0;
    XMFLOAT3 Position   = {};
    XMFLOAT4 Orientation = { 0, 0, 0, 1 };
    XMFLOAT3 Velocity   = {};
  };

  struct StateSnapshotMsg
  {
    uint32_t ServerTick = 0;
    std::vector<EntityState> Entities;

    void Write(DataWriter& _w) const
    {
      _w.Write<uint32_t>(ServerTick);
      uint16_t count = static_cast<uint16_t>(Entities.size());
      _w.Write<uint16_t>(count);
      for (const auto& e : Entities)
      {
        _w.Write<uint32_t>(e.HandleId);
        _w.Write<XMFLOAT3>(e.Position);
        _w.Write<XMFLOAT4>(e.Orientation);
        _w.Write<XMFLOAT3>(e.Velocity);
      }
    }

    void Read(DataReader& _r)
    {
      ServerTick = _r.Read<uint32_t>();
      uint16_t count = _r.Read<uint16_t>();
      Entities.resize(count);
      for (auto& e : Entities)
      {
        e.HandleId    = _r.Read<uint32_t>();
        e.Position    = _r.Read<XMFLOAT3>();
        e.Orientation = _r.Read<XMFLOAT4>();
        e.Velocity    = _r.Read<XMFLOAT3>();
      }
    }
  };

  // ------------------------------------------------------------------
  // PlayerCommand (client → server, reliable)
  // ------------------------------------------------------------------
  enum class CommandType : uint8_t
  {
    MoveTo = 0,
    AttackTarget,
    Dock,
    Loot,
    UseAbility,
    WarpToJumpgate,
  };

  struct PlayerCommandMsg
  {
    CommandType Type      = CommandType::MoveTo;
    EntityHandle TargetHandle;           // Target entity (if applicable)
    XMFLOAT3    TargetPosition = {};     // Destination (for MoveTo)

    void Write(DataWriter& _w) const
    {
      _w.Write<uint8_t>(static_cast<uint8_t>(Type));
      _w.Write<uint32_t>(TargetHandle.m_id);
      _w.Write<XMFLOAT3>(TargetPosition);
    }

    void Read(DataReader& _r)
    {
      Type               = static_cast<CommandType>(_r.Read<uint8_t>());
      TargetHandle.m_id  = _r.Read<uint32_t>();
      TargetPosition     = _r.Read<XMFLOAT3>();
    }
  };

  // ------------------------------------------------------------------
  // ChatMessage (either direction, reliable)
  // ------------------------------------------------------------------
  enum class ChatChannel : uint8_t
  {
    Zone = 0,
    Party,
    Whisper,
    System,
  };

  struct ChatMsg
  {
    ChatChannel Channel    = ChatChannel::Zone;
    std::string SenderName;
    std::string Text;

    void Write(DataWriter& _w) const
    {
      _w.Write<uint8_t>(static_cast<uint8_t>(Channel));
      _w.WriteString(SenderName);
      _w.WriteString(Text);
    }

    void Read(DataReader& _r)
    {
      Channel    = static_cast<ChatChannel>(_r.Read<uint8_t>());
      SenderName = _r.ReadString();
      Text       = _r.ReadString();
    }
  };
}
