#pragma once

// ---------------------------------------------------------------------------
// NetLib.h — Network protocol primitives for Earthrise
//
// Defines the MTU-safe datagram size, packet header, and message ID enum.
// Full UDP socket implementation will be added in Phase 4.
// ---------------------------------------------------------------------------

namespace Neuron::Net
{
  // Maximum payload size per UDP datagram (MTU-safe, excludes IP/UDP headers).
  inline constexpr size_t DATALOAD_SIZE = 1400;

  // Protocol version — bump when packet format changes.
  inline constexpr uint16_t PROTOCOL_VERSION = 1;

  // ---------------------------------------------------------------------------
  // Message IDs — identifies the type of payload in a packet.
  // Expanded in Phase 4 when full networking is implemented.
  // ---------------------------------------------------------------------------
  enum class MessageId : uint8_t
  {
    None = 0,

    // Connection lifecycle
    LoginRequest,
    LoginResponse,
    Disconnect,
    Heartbeat,

    // Entity replication
    EntitySpawn,
    EntityDespawn,
    StateSnapshot,

    // Player input
    PlayerCommand,

    // Chat
    ChatMessage,

    COUNT
  };
  ENUM_HELPER(MessageId, None, ChatMessage)

  // ---------------------------------------------------------------------------
  // PacketHeader — prepended to every datagram.
  // Total header size: 8 bytes (leaves 1392 bytes for payload).
  // ---------------------------------------------------------------------------
  struct PacketHeader
  {
    uint16_t ProtocolVersion = PROTOCOL_VERSION;
    MessageId MsgId          = MessageId::None;
    uint8_t Flags            = 0;       // Bit 0 = reliable, others reserved
    uint16_t Sequence        = 0;       // Per-channel sequence number
    uint16_t PayloadSize     = 0;       // Bytes of payload following this header

    static constexpr size_t SIZE = 8;
    static constexpr size_t MAX_PAYLOAD = DATALOAD_SIZE - SIZE;

    // Flag helpers
    static constexpr uint8_t FLAG_RELIABLE = 0x01;

    [[nodiscard]] bool IsReliable() const noexcept { return (Flags & FLAG_RELIABLE) != 0; }
  };
  static_assert(sizeof(PacketHeader) == PacketHeader::SIZE, "PacketHeader must be exactly 8 bytes");
}

// Expose DATALOAD_SIZE at global scope for DataReader / DataWriter compatibility.
inline constexpr size_t DATALOAD_SIZE = Neuron::Net::DATALOAD_SIZE;
