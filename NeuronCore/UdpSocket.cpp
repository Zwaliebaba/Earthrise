#include "pch.h"
#include "UdpSocket.h"

namespace Neuron
{
  static bool s_winsockInitialized = false;

  void InitializeNetworking()
  {
    if (s_winsockInitialized)
      return;

    WSADATA wsaData{};
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
      Fatal("WSAStartup failed with error: {}", result);

    s_winsockInitialized = true;
    DebugTrace("Networking initialized (Winsock {}.{})\n",
               LOBYTE(wsaData.wVersion), HIBYTE(wsaData.wVersion));
  }

  void ShutdownNetworking()
  {
    if (!s_winsockInitialized)
      return;

    WSACleanup();
    s_winsockInitialized = false;
    DebugTrace("Networking shut down\n");
  }

  UdpSocket::~UdpSocket()
  {
    Close();
  }

  UdpSocket::UdpSocket(UdpSocket&& _other) noexcept
    : m_socket(_other.m_socket)
  {
    _other.m_socket = INVALID_SOCKET;
  }

  UdpSocket& UdpSocket::operator=(UdpSocket&& _other) noexcept
  {
    if (this != &_other)
    {
      Close();
      m_socket = _other.m_socket;
      _other.m_socket = INVALID_SOCKET;
    }
    return *this;
  }

  bool UdpSocket::Bind(uint16_t _port)
  {
    Close();

    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET)
    {
      DebugTrace("socket() failed: {}\n", WSAGetLastError());
      return false;
    }

    // Set non-blocking mode
    u_long nonBlocking = 1;
    if (ioctlsocket(m_socket, FIONBIO, &nonBlocking) == SOCKET_ERROR)
    {
      DebugTrace("ioctlsocket(FIONBIO) failed: {}\n", WSAGetLastError());
      Close();
      return false;
    }

    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(_port);
    bindAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(m_socket, reinterpret_cast<const sockaddr*>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR)
    {
      DebugTrace("bind() failed on port {}: {}\n", _port, WSAGetLastError());
      Close();
      return false;
    }

    DebugTrace("UDP socket bound to port {}\n", _port);
    return true;
  }

  int UdpSocket::SendTo(const void* _data, int _size, const sockaddr_in& _addr) const
  {
    ASSERT(IsValid());
    ASSERT(_data != nullptr);
    ASSERT(_size > 0 && _size <= static_cast<int>(DATALOAD_SIZE));

    int sent = sendto(m_socket, static_cast<const char*>(_data), _size, 0,
                      reinterpret_cast<const sockaddr*>(&_addr), sizeof(_addr));
    if (sent == SOCKET_ERROR)
    {
      int err = WSAGetLastError();
      if (err != WSAEWOULDBLOCK)
        DebugTrace("sendto() failed: {}\n", err);
      return -1;
    }
    return sent;
  }

  int UdpSocket::RecvFrom(void* _data, int _maxSize, sockaddr_in& _addr) const
  {
    ASSERT(IsValid());
    ASSERT(_data != nullptr);

    int addrLen = sizeof(_addr);
    int received = recvfrom(m_socket, static_cast<char*>(_data), _maxSize, 0,
                            reinterpret_cast<sockaddr*>(&_addr), &addrLen);
    if (received == SOCKET_ERROR)
    {
      int err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK)
        return 0; // No data available
      DebugTrace("recvfrom() failed: {}\n", err);
      return -1;
    }
    return received;
  }

  void UdpSocket::Close()
  {
    if (m_socket != INVALID_SOCKET)
    {
      closesocket(m_socket);
      m_socket = INVALID_SOCKET;
    }
  }

  sockaddr_in UdpSocket::MakeAddress(const char* _ip, uint16_t _port)
  {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    inet_pton(AF_INET, _ip, &addr.sin_addr);
    return addr;
  }
}
