#include "pch.h"
#include "ClientSession.h"

namespace Neuron::Server
{
  ClientSession::ClientSession(uint32_t _sessionId, const sockaddr_in& _addr)
    : m_sessionId(_sessionId)
    , m_addr(_addr)
  {
  }

  int ClientSession::SendPacket(const UdpSocket& _socket, MessageId _msgId, uint8_t _flags,
                                const void* _payload, int _payloadSize)
  {
    int framed = FramePacket(_msgId, m_unreliableSeq++, _flags,
                             _payload, _payloadSize,
                             m_sendBuffer.data(), static_cast<int>(m_sendBuffer.size()));
    if (framed <= 0)
      return -1;

    return _socket.SendTo(m_sendBuffer.data(), framed, m_addr);
  }

  int ClientSession::SendReliable(const UdpSocket& _socket, MessageId _msgId,
                                  const void* _payload, int _payloadSize, double _currentTime)
  {
    // Frame with reliable flag and the reliability layer's sequence number.
    uint16_t seq = m_reliable.NextSendSequence();
    int framed = FramePacket(_msgId, seq, PACKET_FLAG_RELIABLE,
                             _payload, _payloadSize,
                             m_sendBuffer.data(), static_cast<int>(m_sendBuffer.size()));
    if (framed <= 0)
      return -1;

    // Queue for ACK tracking (stores the fully framed datagram).
    m_reliable.QueueReliable(m_sendBuffer.data(), framed, _currentTime);

    return _socket.SendTo(m_sendBuffer.data(), framed, m_addr);
  }
}
