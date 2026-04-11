#pragma once

#include "UdpSocket.h"
#include "ReliableChannel.h"
#include "Packet.h"
#include "Messages.h"

namespace Neuron::Server
{
  // Per-client session state on the server.
  class ClientSession
  {
  public:
    enum class State : uint8_t
    {
      Connecting,   // LoginRequest received, awaiting processing
      Connected,    // Fully authenticated and active
      Disconnecting // Disconnect in progress
    };

    ClientSession(uint32_t _sessionId, const sockaddr_in& _addr);

    [[nodiscard]] uint32_t    SessionId() const noexcept { return m_sessionId; }
    [[nodiscard]] State       GetState() const noexcept { return m_state; }
    [[nodiscard]] const std::string& PlayerName() const noexcept { return m_playerName; }
    [[nodiscard]] const sockaddr_in& Address() const noexcept { return m_addr; }
    [[nodiscard]] ReliableChannel& Reliable() noexcept { return m_reliable; }
    [[nodiscard]] double      LastActivityTime() const noexcept { return m_lastActivityTime; }

    void SetState(State _state) noexcept { m_state = _state; }
    void SetPlayerName(std::string _name) { m_playerName = std::move(_name); }
    void TouchActivity(double _time) noexcept { m_lastActivityTime = _time; }

    // Send a framed packet to this client via the shared server socket.
    int SendPacket(const UdpSocket& _socket, MessageId _msgId, uint8_t _flags,
                   const void* _payload, int _payloadSize);

    // Send a reliable message (queued for ACK tracking).
    int SendReliable(const UdpSocket& _socket, MessageId _msgId,
                     const void* _payload, int _payloadSize, double _currentTime);

  private:
    uint32_t         m_sessionId;
    sockaddr_in      m_addr{};
    State            m_state = State::Connecting;
    std::string      m_playerName;
    double           m_lastActivityTime = 0.0;
    ReliableChannel  m_reliable;
    uint16_t         m_unreliableSeq = 0;

    // Shared send buffer (avoids per-call allocation)
    std::array<uint8_t, DATALOAD_SIZE> m_sendBuffer{};
  };
}
