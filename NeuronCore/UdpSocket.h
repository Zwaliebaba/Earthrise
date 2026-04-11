#pragma once

namespace Neuron
{
  // Initialize / shut down Winsock. Call once at app startup/shutdown.
  void InitializeNetworking();
  void ShutdownNetworking();

  // Lightweight non-blocking UDP socket wrapper over Winsock2.
  class UdpSocket
  {
  public:
    UdpSocket() = default;
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&& _other) noexcept;
    UdpSocket& operator=(UdpSocket&& _other) noexcept;

    // Bind to a local port. Pass 0 for an ephemeral port.
    bool Bind(uint16_t _port);

    // Send a datagram to the specified address. Returns bytes sent, or -1 on error.
    int SendTo(const void* _data, int _size, const sockaddr_in& _addr) const;

    // Receive a datagram. Fills _addr with the sender. Returns bytes received,
    // 0 if no data available (non-blocking), or -1 on error.
    int RecvFrom(void* _data, int _maxSize, sockaddr_in& _addr) const;

    // Close the socket and release system resources.
    void Close();

    [[nodiscard]] bool IsValid() const noexcept { return m_socket != INVALID_SOCKET; }
    [[nodiscard]] SOCKET GetHandle() const noexcept { return m_socket; }

    // Build a sockaddr_in from IP string and port.
    static sockaddr_in MakeAddress(const char* _ip, uint16_t _port);

  private:
    SOCKET m_socket = INVALID_SOCKET;
  };
}
