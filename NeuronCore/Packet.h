#pragma once

namespace Neuron
{
  // Maximum payload size (datagram minus header).
  constexpr int MAX_PAYLOAD_SIZE = static_cast<int>(DATALOAD_SIZE - sizeof(PacketHeader));

  // Frame a message into a raw datagram buffer.
  // Returns total bytes written (header + payload), or -1 on error.
  [[nodiscard]] inline int FramePacket(
    MessageId _msgId,
    uint16_t  _seq,
    uint8_t   _flags,
    const void* _payload,
    int         _payloadSize,
    void*       _outBuffer,
    int         _bufferSize) noexcept
  {
    const int totalSize = static_cast<int>(sizeof(PacketHeader)) + _payloadSize;
    if (_payloadSize < 0 || _payloadSize > MAX_PAYLOAD_SIZE || totalSize > _bufferSize)
      return -1;

    PacketHeader header{};
    header.MessageId      = static_cast<uint16_t>(_msgId);
    header.SequenceNumber = _seq;
    header.PayloadSize    = static_cast<uint16_t>(_payloadSize);
    header.Flags          = _flags;
    header.Reserved       = 0;

    std::memcpy(_outBuffer, &header, sizeof(PacketHeader));
    if (_payloadSize > 0 && _payload != nullptr)
      std::memcpy(static_cast<uint8_t*>(_outBuffer) + sizeof(PacketHeader), _payload, _payloadSize);

    return totalSize;
  }

  // Parse a raw datagram into header and payload pointer.
  // Returns true if the datagram is well-formed.
  [[nodiscard]] inline bool ParsePacket(
    const void*   _data,
    int           _size,
    PacketHeader& _outHeader,
    const uint8_t*& _outPayload,
    int&          _outPayloadSize) noexcept
  {
    if (_data == nullptr || _size < static_cast<int>(sizeof(PacketHeader)))
      return false;

    std::memcpy(&_outHeader, _data, sizeof(PacketHeader));

    _outPayloadSize = static_cast<int>(_outHeader.PayloadSize);
    if (_outPayloadSize < 0 || static_cast<int>(sizeof(PacketHeader)) + _outPayloadSize > _size)
      return false;

    _outPayload = static_cast<const uint8_t*>(_data) + sizeof(PacketHeader);
    return true;
  }
}
