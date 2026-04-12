#include "pch.h"
#include "ServerConnection.h"

ServerConnection::~ServerConnection()
{
  Disconnect();
}

bool ServerConnection::Connect(const char* _ip, uint16_t _port, const std::string& _playerName)
{
  if (m_state.load(std::memory_order_acquire) != State::Disconnected)
    return false;

  Neuron::InitializeNetworking();

  if (!m_socket.Bind(0)) // ephemeral port
    return false;

  m_serverAddr = Neuron::UdpSocket::MakeAddress(_ip, _port);
  m_playerName = _playerName;
  m_state.store(State::Connecting, std::memory_order_release);

  // Send login request via reliable channel.
  {
    Neuron::DataWriter writer;
    Neuron::LoginRequest req;
    req.PlayerName      = _playerName;
    req.ProtocolVersion = Neuron::ENGINE_VERSION;
    req.Write(writer);

    SendReliable(Neuron::MessageId::LoginRequest, writer.Data(), writer.Size());
  }

  // Start background receive thread.
  m_running.store(true, std::memory_order_release);
  m_thread = std::thread(&ServerConnection::NetworkThread, this);

  return true;
}

void ServerConnection::Disconnect()
{
  if (m_state.load(std::memory_order_acquire) == State::Disconnected)
    return;

  m_state.store(State::Disconnecting, std::memory_order_release);

  // Send disconnect message (best-effort).
  Neuron::DataWriter writer;
  Neuron::DisconnectMsg msg;
  msg.Reason = Neuron::DisconnectReason::Graceful;
  msg.Write(writer);
  SendReliable(Neuron::MessageId::Disconnect, writer.Data(), writer.Size());

  // Stop background thread.
  m_running.store(false, std::memory_order_release);
  if (m_thread.joinable())
    m_thread.join();

  m_socket.Close();
  m_sessionId = 0;
  m_state.store(State::Disconnected, std::memory_order_release);
}

int ServerConnection::Send(Neuron::MessageId _msgId, const void* _payload, int _payloadSize)
{
  std::lock_guard lock(m_sendMutex);

  int framed = Neuron::FramePacket(_msgId, m_unreliableSeq++, 0,
                                   _payload, _payloadSize,
                                   m_sendBuffer.data(),
                                   static_cast<int>(m_sendBuffer.size()));
  if (framed <= 0)
    return -1;

  return m_socket.SendTo(m_sendBuffer.data(), framed, m_serverAddr);
}

int ServerConnection::SendReliable(Neuron::MessageId _msgId, const void* _payload, int _payloadSize)
{
  std::lock_guard lock(m_sendMutex);

  uint16_t seq = m_reliable.NextSendSequence();
  int framed = Neuron::FramePacket(_msgId, seq, Neuron::PACKET_FLAG_RELIABLE,
                                   _payload, _payloadSize,
                                   m_sendBuffer.data(),
                                   static_cast<int>(m_sendBuffer.size()));
  if (framed <= 0)
    return -1;

  double now = Neuron::Timer::Core::GetTotalSeconds();
  m_reliable.QueueReliable(m_sendBuffer.data(), framed, now);

  return m_socket.SendTo(m_sendBuffer.data(), framed, m_serverAddr);
}

void ServerConnection::DrainMessages(std::vector<ReceivedMessage>& _outMessages, size_t _maxMessages)
{
  ReceivedMessage msg;
  for (size_t i = 0; i < _maxMessages; ++i)
  {
    if (!m_incomingQueue.try_pop(msg))
      break;
    _outMessages.push_back(std::move(msg));
  }
}

void ServerConnection::NetworkThread()
{
  while (m_running.load(std::memory_order_acquire))
  {
    // Receive datagrams.
    sockaddr_in senderAddr{};
    int bytes = m_socket.RecvFrom(m_recvBuffer.data(),
                                  static_cast<int>(m_recvBuffer.size()),
                                  senderAddr);
    if (bytes > 0)
    {
      Neuron::PacketHeader header{};
      const uint8_t* payload = nullptr;
      int payloadSize = 0;

      if (Neuron::ParsePacket(m_recvBuffer.data(), bytes, header, payload, payloadSize))
      {
        auto msgId = static_cast<Neuron::MessageId>(header.MessageId);

        // If reliable, send ACK and check for duplicates.
        if (header.Flags & Neuron::PACKET_FLAG_RELIABLE)
        {
          std::array<uint8_t, Neuron::DATALOAD_SIZE> ackBuf{};
          int ackSize = Neuron::ReliableChannel::BuildAck(
            header.SequenceNumber, ackBuf.data(),
            static_cast<int>(ackBuf.size()));
          if (ackSize > 0)
            m_socket.SendTo(ackBuf.data(), ackSize, m_serverAddr);

          if (m_reliable.IsDuplicate(header.SequenceNumber))
            continue;
          m_reliable.MarkReceived(header.SequenceNumber);
        }

        // Handle ACK internally.
        if (msgId == Neuron::MessageId::Ack)
        {
          std::lock_guard lock(m_sendMutex);
          m_reliable.Acknowledge(header.SequenceNumber);
          continue;
        }

        // Handle login response internally to update state.
        if (msgId == Neuron::MessageId::LoginResponse)
        {
          Neuron::DataReader reader(payload, payloadSize);
          Neuron::LoginResponse resp;
          resp.Read(reader);

          if (resp.Result == Neuron::LoginResult::Success)
          {
            m_sessionId = resp.SessionId;
            m_state.store(State::Connected, std::memory_order_release);
            Neuron::DebugTrace("ServerConnection: connected, session {}\n", m_sessionId);
          }
          else
          {
            Neuron::DebugTrace("ServerConnection: login rejected ({})\n",
                               static_cast<int>(resp.Result));
            m_state.store(State::Disconnected, std::memory_order_release);
            m_running.store(false, std::memory_order_release);
          }
          continue;
        }

        // Queue everything else for main thread.
        ReceivedMessage msg;
        msg.MsgId = msgId;
        msg.Payload.assign(payload, payload + payloadSize);
        m_incomingQueue.push(std::move(msg));
      }
    }

    // Resend timed-out reliable packets.
    {
      std::lock_guard lock(m_sendMutex);
      double now = Neuron::Timer::Core::GetTotalSeconds();
      std::vector<Neuron::PendingPacket> resends;
      m_reliable.CollectResends(now, resends);
      for (const auto& pkt : resends)
        m_socket.SendTo(pkt.Data.data(), pkt.Size, m_serverAddr);

      // If the login request was dropped after MAX_RESENDS, re-queue it.
      RetryLogin(now);
    }

    // Yield to avoid busy-spinning when there's no data.
    std::this_thread::yield();
  }
}

void ServerConnection::RetryLogin(double _now)
{
  if (m_state.load(std::memory_order_acquire) != State::Connecting)
    return;

  if (m_reliable.PendingCount() > 0)
    return; // Still retrying via the reliable channel

  if (_now < m_nextRetryTime)
    return;

  Neuron::DebugTrace("ServerConnection: retrying login...\n");

  Neuron::DataWriter writer;
  Neuron::LoginRequest req;
  req.PlayerName      = m_playerName;
  req.ProtocolVersion = Neuron::ENGINE_VERSION;
  req.Write(writer);

  uint16_t seq = m_reliable.NextSendSequence();
  int framed = Neuron::FramePacket(Neuron::MessageId::LoginRequest, seq,
                                   Neuron::PACKET_FLAG_RELIABLE,
                                   writer.Data(), writer.Size(),
                                   m_sendBuffer.data(),
                                   static_cast<int>(m_sendBuffer.size()));
  if (framed > 0)
  {
    m_reliable.QueueReliable(m_sendBuffer.data(), framed, _now);
    m_socket.SendTo(m_sendBuffer.data(), framed, m_serverAddr);
  }

  m_nextRetryTime = _now + RETRY_INTERVAL;
}
