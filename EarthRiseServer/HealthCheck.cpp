#include "pch.h"
#include "HealthCheck.h"

namespace EarthRise
{
  bool HealthCheck::Start(uint16_t port)
  {
    if (m_running.load())
      return false;

    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET)
      return false;

    // Allow address reuse for fast restarts.
    int opt = 1;
    setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
      closesocket(m_listenSocket);
      m_listenSocket = INVALID_SOCKET;
      return false;
    }

    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
      closesocket(m_listenSocket);
      m_listenSocket = INVALID_SOCKET;
      return false;
    }

    // Resolve actual port (for port-0 auto-assignment).
    sockaddr_in bound{};
    int boundLen = sizeof(bound);
    if (getsockname(m_listenSocket, reinterpret_cast<sockaddr*>(&bound), &boundLen) == 0)
      m_port = ntohs(bound.sin_port);
    else
      m_port = port;

    m_running.store(true);
    m_thread = std::thread(&HealthCheck::ListenLoop, this);
    return true;
  }

  void HealthCheck::Stop()
  {
    m_running.store(false);

    // Close the listen socket to unblock select/accept.
    if (m_listenSocket != INVALID_SOCKET)
    {
      closesocket(m_listenSocket);
      m_listenSocket = INVALID_SOCKET;
    }

    if (m_thread.joinable())
      m_thread.join();
  }

  void HealthCheck::ListenLoop()
  {
    while (m_running.load())
    {
      fd_set readSet;
      FD_ZERO(&readSet);
      FD_SET(m_listenSocket, &readSet);

      timeval timeout{};
      timeout.tv_sec  = 0;
      timeout.tv_usec = 100'000; // 100 ms

      int ready = select(0, &readSet, nullptr, nullptr, &timeout);
      if (ready <= 0 || !m_running.load())
        continue;

      SOCKET client = accept(m_listenSocket, nullptr, nullptr);
      if (client == INVALID_SOCKET)
        continue;

      // Send health response and close immediately.
      send(client, HTTP_RESPONSE,
           static_cast<int>(std::strlen(HTTP_RESPONSE)), 0);
      closesocket(client);
    }
  }
}
