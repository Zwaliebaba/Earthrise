#pragma once

namespace Neuron
{
  // Maximum payload size for a single UDP datagram (MTU-safe).
  // Ethernet MTU = 1500 bytes - 20 bytes IP header - 8 bytes UDP header = 1472 bytes.
  // We use 1400 to leave margin for IP options and tunneling overhead.
  constexpr size_t DATALOAD_SIZE = 1400;

  // Packet header precedes every datagram payload.
  struct PacketHeader
  {
    uint16_t MessageId;       // Identifies the message type (see MessageId enum)
    uint16_t SequenceNumber;  // Monotonically increasing per-sender for ordering / ACK
    uint16_t PayloadSize;     // Size of the payload following this header
    uint8_t  Flags;           // Bit 0 = reliable, remaining reserved
    uint8_t  Reserved;        // Padding for alignment
  };
  static_assert(sizeof(PacketHeader) == 8, "PacketHeader must be 8 bytes");

  // Channel flag bits for PacketHeader::Flags
  constexpr uint8_t PACKET_FLAG_RELIABLE = 0x01;

  // Core message IDs for the client ↔ server protocol.
  // Gameplay-specific messages will be added in later phases.
  enum class MessageId : uint16_t
  {
    // Connection lifecycle
    LoginRequest        = 0x0001,
    LoginResponse       = 0x0002,
    Disconnect          = 0x0003,
    Heartbeat           = 0x0004,

    // Entity state
    EntitySpawn         = 0x0010,
    EntityDespawn       = 0x0011,
    StateSnapshot       = 0x0012,

    // Player input
    PlayerCommand       = 0x0020,

    // Chat
    ChatMessage         = 0x0030,

    // Acknowledgement (reliability layer)
    Ack                 = 0x00FF,
  };
}
