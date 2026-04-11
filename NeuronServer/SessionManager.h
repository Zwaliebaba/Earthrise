#pragma once

#include "ClientSession.h"

namespace Neuron::Server
{
  // Manages all active client sessions on the server side.
  // Owns the server UDP socket, dispatches incoming datagrams
  // to the appropriate ClientSession, and handles session lifecycle.
  class SessionManager
  {
  public:
    // Maximum concurrent clients.
    static constexpr uint32_t MAX_CLIENTS = 100;
    // Timeout in seconds before a session is considered dead.
    static constexpr double   SESSION_TIMEOUT = 30.0;

    SessionManager() = default;

    // Initialize the server socket on the specified port.
    bool Startup(uint16_t _port);

    // Shut down all sessions and close the socket.
    void Shutdown();

    // Poll for incoming datagrams and process them.
    // Call once per server tick.
    void Poll(double _currentTime);

    // Resend any timed-out reliable packets across all sessions.
    void ResendReliable(double _currentTime);

    // Remove sessions that have timed out.
    void PruneTimedOut(double _currentTime);

    // Send a packet to a specific session.
    int SendTo(uint32_t _sessionId, MessageId _msgId, uint8_t _flags,
               const void* _payload, int _payloadSize);

    // Send a reliable packet to a specific session.
    int SendReliable(uint32_t _sessionId, MessageId _msgId,
                     const void* _payload, int _payloadSize, double _currentTime);

    // Broadcast an unreliable packet to all connected sessions.
    void Broadcast(MessageId _msgId, const void* _payload, int _payloadSize);

    [[nodiscard]] size_t SessionCount() const noexcept { return m_sessions.size(); }
    [[nodiscard]] ClientSession* FindSession(uint32_t _sessionId);
    [[nodiscard]] const UdpSocket& Socket() const noexcept { return m_socket; }

  private:
    // Look up a session by remote address. Returns nullptr if not found.
    ClientSession* FindByAddress(const sockaddr_in& _addr);

    // Create a new session for the given address.
    ClientSession* CreateSession(const sockaddr_in& _addr, double _currentTime);

    // Process a single received datagram.
    void ProcessDatagram(const uint8_t* _data, int _size,
                         const sockaddr_in& _senderAddr, double _currentTime);

    // Handle individual message types.
    void HandleLoginRequest(ClientSession& _session, const uint8_t* _payload,
                            int _payloadSize, double _currentTime);
    void HandleDisconnect(ClientSession& _session, const uint8_t* _payload, int _payloadSize);
    void HandleHeartbeat(ClientSession& _session, double _currentTime);
    void HandleAck(ClientSession& _session, uint16_t _seq);

    UdpSocket m_socket;
    uint32_t  m_nextSessionId = 1;

    // Sessions keyed by session ID.
    std::unordered_map<uint32_t, std::unique_ptr<ClientSession>> m_sessions;

    // Address-to-session-ID lookup for fast dispatch of incoming datagrams.
    // Key is packed from IP + port.
    std::unordered_map<uint64_t, uint32_t> m_addrToSession;

    // Receive buffer (shared across all recv calls).
    std::array<uint8_t, DATALOAD_SIZE> m_recvBuffer{};

    // Helper: pack a sockaddr_in into a uint64_t key.
    static uint64_t AddressKey(const sockaddr_in& _addr) noexcept
    {
      return (static_cast<uint64_t>(_addr.sin_addr.s_addr) << 16)
           | static_cast<uint64_t>(_addr.sin_port);
    }
  };
}
