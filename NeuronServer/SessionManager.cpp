#include "pch.h"
#include "SessionManager.h"

namespace Neuron::Server
{
  bool SessionManager::Startup(uint16_t _port)
  {
    InitializeNetworking();
    if (!m_socket.Bind(_port))
    {
      DebugTrace("SessionManager: failed to bind on port {}\n", _port);
      return false;
    }
    DebugTrace("SessionManager: listening on port {}\n", _port);
    return true;
  }

  void SessionManager::Shutdown()
  {
    m_sessions.clear();
    m_addrToSession.clear();
    m_socket.Close();
    DebugTrace("SessionManager: shut down\n");
  }

  void SessionManager::Poll(double _currentTime)
  {
    // Drain all pending datagrams (non-blocking).
    for (;;)
    {
      sockaddr_in senderAddr{};
      int bytes = m_socket.RecvFrom(m_recvBuffer.data(),
                                    static_cast<int>(m_recvBuffer.size()),
                                    senderAddr);
      if (bytes <= 0)
        break;

      ProcessDatagram(m_recvBuffer.data(), bytes, senderAddr, _currentTime);
    }
  }

  void SessionManager::ResendReliable(double _currentTime)
  {
    std::vector<PendingPacket> resends;

    for (auto& [id, session] : m_sessions)
    {
      resends.clear();
      session->Reliable().CollectResends(_currentTime, resends);
      for (const auto& pkt : resends)
      {
        m_socket.SendTo(pkt.Data.data(), pkt.Size, session->Address());
      }
    }
  }

  void SessionManager::PruneTimedOut(double _currentTime)
  {
    std::vector<uint32_t> deadIds;
    for (auto& [id, session] : m_sessions)
    {
      if (_currentTime - session->LastActivityTime() > SESSION_TIMEOUT)
        deadIds.push_back(id);
    }

    for (uint32_t id : deadIds)
    {
      auto it = m_sessions.find(id);
      if (it != m_sessions.end())
      {
        uint64_t key = AddressKey(it->second->Address());
        m_addrToSession.erase(key);
        DebugTrace("SessionManager: session {} timed out\n", id);
        m_sessions.erase(it);
      }
    }
  }

  int SessionManager::SendTo(uint32_t _sessionId, MessageId _msgId, uint8_t _flags,
                              const void* _payload, int _payloadSize)
  {
    auto* session = FindSession(_sessionId);
    if (!session)
      return -1;
    return session->SendPacket(m_socket, _msgId, _flags, _payload, _payloadSize);
  }

  int SessionManager::SendReliable(uint32_t _sessionId, MessageId _msgId,
                                   const void* _payload, int _payloadSize, double _currentTime)
  {
    auto* session = FindSession(_sessionId);
    if (!session)
      return -1;
    return session->SendReliable(m_socket, _msgId, _payload, _payloadSize, _currentTime);
  }

  void SessionManager::Broadcast(MessageId _msgId, const void* _payload, int _payloadSize)
  {
    for (auto& [id, session] : m_sessions)
    {
      if (session->GetState() == ClientSession::State::Connected)
        session->SendPacket(m_socket, _msgId, 0, _payload, _payloadSize);
    }
  }

  ClientSession* SessionManager::FindSession(uint32_t _sessionId)
  {
    auto it = m_sessions.find(_sessionId);
    return (it != m_sessions.end()) ? it->second.get() : nullptr;
  }

  ClientSession* SessionManager::FindByAddress(const sockaddr_in& _addr)
  {
    uint64_t key = AddressKey(_addr);
    auto it = m_addrToSession.find(key);
    if (it == m_addrToSession.end())
      return nullptr;
    return FindSession(it->second);
  }

  ClientSession* SessionManager::CreateSession(const sockaddr_in& _addr, double _currentTime)
  {
    if (m_sessions.size() >= MAX_CLIENTS)
    {
      DebugTrace("SessionManager: server full, rejecting new connection\n");
      return nullptr;
    }

    uint32_t id = m_nextSessionId++;
    auto session = std::make_unique<ClientSession>(id, _addr);
    session->TouchActivity(_currentTime);

    uint64_t key = AddressKey(_addr);
    m_addrToSession[key] = id;

    auto* ptr = session.get();
    m_sessions[id] = std::move(session);

    DebugTrace("SessionManager: created session {} for {}:{}\n",
               id, inet_ntoa(_addr.sin_addr), ntohs(_addr.sin_port));
    return ptr;
  }

  void SessionManager::ProcessDatagram(const uint8_t* _data, int _size,
                                       const sockaddr_in& _senderAddr, double _currentTime)
  {
    PacketHeader header{};
    const uint8_t* payload = nullptr;
    int payloadSize = 0;

    if (!ParsePacket(_data, _size, header, payload, payloadSize))
    {
      DebugTrace("SessionManager: malformed datagram dropped\n");
      return;
    }

    auto msgId = static_cast<MessageId>(header.MessageId);

    // Look up or create session.
    ClientSession* session = FindByAddress(_senderAddr);
    if (!session)
    {
      // Only accept LoginRequest from unknown addresses.
      if (msgId != MessageId::LoginRequest)
        return;

      session = CreateSession(_senderAddr, _currentTime);
      if (!session)
        return;
    }

    session->TouchActivity(_currentTime);

    // Reliable message deduplication.
    bool reliable = (header.Flags & PACKET_FLAG_RELIABLE) != 0;
    if (reliable)
    {
      // Always send ACK, even for duplicates.
      std::array<uint8_t, DATALOAD_SIZE> ackBuf{};
      int ackSize = ReliableChannel::BuildAck(header.SequenceNumber,
                                               ackBuf.data(),
                                               static_cast<int>(ackBuf.size()));
      if (ackSize > 0)
        m_socket.SendTo(ackBuf.data(), ackSize, _senderAddr);

      if (session->Reliable().IsDuplicate(header.SequenceNumber))
        return; // Already processed
      session->Reliable().MarkReceived(header.SequenceNumber);
    }

    // Dispatch by message type.
    switch (msgId)
    {
    case MessageId::LoginRequest:
      HandleLoginRequest(*session, payload, payloadSize, _currentTime);
      break;
    case MessageId::Disconnect:
      HandleDisconnect(*session, payload, payloadSize);
      break;
    case MessageId::Heartbeat:
      HandleHeartbeat(*session, _currentTime);
      break;
    case MessageId::Ack:
      HandleAck(*session, header.SequenceNumber);
      break;
    default:
      // Gameplay messages will be dispatched in later phases.
      break;
    }
  }

  void SessionManager::HandleLoginRequest(ClientSession& _session, const uint8_t* _payload,
                                          int _payloadSize, double _currentTime)
  {
    DataReader reader(_payload, _payloadSize);
    LoginRequest req;
    req.Read(reader);

    _session.SetPlayerName(std::move(req.PlayerName));
    _session.SetState(ClientSession::State::Connected);

    DebugTrace("SessionManager: player '{}' logged in (session {})\n",
               _session.PlayerName(), _session.SessionId());

    // Build and send login response.
    LoginResponse resp;
    resp.Result    = (req.ProtocolVersion == ENGINE_VERSION)
                       ? LoginResult::Success
                       : LoginResult::VersionMismatch;
    resp.SessionId = _session.SessionId();

    DataWriter writer;
    resp.Write(writer);

    _session.SendReliable(m_socket, MessageId::LoginResponse,
                          writer.Data(), writer.Size(), _currentTime);
  }

  void SessionManager::HandleDisconnect(ClientSession& _session, const uint8_t* _payload,
                                        int _payloadSize)
  {
    (void)_payload;
    (void)_payloadSize;

    DebugTrace("SessionManager: session {} disconnected\n", _session.SessionId());
    _session.SetState(ClientSession::State::Disconnecting);

    // Remove immediately.
    uint64_t key = AddressKey(_session.Address());
    m_addrToSession.erase(key);
    m_sessions.erase(_session.SessionId());
  }

  void SessionManager::HandleHeartbeat(ClientSession& _session, double _currentTime)
  {
    _session.TouchActivity(_currentTime);
  }

  void SessionManager::HandleAck(ClientSession& _session, uint16_t _seq)
  {
    _session.Reliable().Acknowledge(_seq);
  }
}
