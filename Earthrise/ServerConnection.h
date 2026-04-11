#pragma once

#include "UdpSocket.h"
#include "ReliableChannel.h"
#include "Packet.h"
#include "Messages.h"

// ServerConnection — client-side networking: connects to a remote server,
// sends/receives on a background thread, and queues incoming messages
// for main-thread consumption.

struct ReceivedMessage
{
  Neuron::MessageId MsgId;
  std::vector<uint8_t> Payload;
};

class ServerConnection
{
public:
  enum class State : uint8_t
  {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting
  };

  ServerConnection() = default;
  ~ServerConnection();

  ServerConnection(const ServerConnection&) = delete;
  ServerConnection& operator=(const ServerConnection&) = delete;

  // Start connecting to the server at _ip:_port.
  // Sends a LoginRequest on the reliable channel.
  bool Connect(const char* _ip, uint16_t _port, const std::string& _playerName);

  // Graceful disconnect.
  void Disconnect();

  // Send an unreliable packet to the server.
  int Send(Neuron::MessageId _msgId, const void* _payload, int _payloadSize);

  // Send a reliable packet to the server.
  int SendReliable(Neuron::MessageId _msgId, const void* _payload, int _payloadSize);

  // Drain the incoming message queue (call from main thread).
  // Returns up to _maxMessages and appends them to _outMessages.
  void DrainMessages(std::vector<ReceivedMessage>& _outMessages, size_t _maxMessages = 64);

  [[nodiscard]] State GetState() const noexcept { return m_state.load(std::memory_order_acquire); }
  [[nodiscard]] uint32_t SessionId() const noexcept { return m_sessionId; }

private:
  void NetworkThread();

  std::atomic<State>   m_state{ State::Disconnected };
  std::atomic<bool>    m_running{ false };
  std::thread          m_thread;

  Neuron::UdpSocket    m_socket;
  sockaddr_in          m_serverAddr{};

  Neuron::ReliableChannel m_reliable;
  uint16_t             m_unreliableSeq = 0;
  uint32_t             m_sessionId = 0;
  std::string          m_playerName;

  // Thread-safe incoming message queue.
  Concurrency::concurrent_queue<ReceivedMessage> m_incomingQueue;

  // Mutex-protected send operations (main thread calls Send while net thread resends).
  std::mutex           m_sendMutex;

  // Shared buffers
  std::array<uint8_t, Neuron::DATALOAD_SIZE> m_sendBuffer{};
  std::array<uint8_t, Neuron::DATALOAD_SIZE> m_recvBuffer{};
};
