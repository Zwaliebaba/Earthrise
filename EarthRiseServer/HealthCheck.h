#pragma once

#include <thread>
#include <atomic>

namespace EarthRise
{
  // TCP health-check endpoint for container orchestration.
  // Listens on a configurable port and responds HTTP 200 to any connection.
  class HealthCheck : Neuron::NonCopyable
  {
  public:
    HealthCheck() = default;
    ~HealthCheck() { Stop(); }

    // Start listening on the given port. Use port 0 for auto-assignment.
    bool Start(uint16_t port);

    // Stop the listener and join the background thread.
    void Stop();

    [[nodiscard]] bool IsRunning() const noexcept { return m_running.load(); }

    // Actual port the listener is bound to (valid after Start).
    [[nodiscard]] uint16_t GetPort() const noexcept { return m_port; }

    // The HTTP response sent to health-check clients.
    static constexpr const char* HTTP_RESPONSE =
      "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK";

  private:
    void ListenLoop();

    std::thread      m_thread;
    std::atomic<bool> m_running{false};
    SOCKET           m_listenSocket = INVALID_SOCKET;
    uint16_t         m_port         = 0;
  };
}
