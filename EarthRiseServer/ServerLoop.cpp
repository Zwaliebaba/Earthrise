#include "pch.h"
#include "ServerLoop.h"

namespace EarthRise
{
  ServerLoop::ServerLoop()
    : m_broadcaster(m_zone, m_sessions, m_bandwidth)
  {
  }

  bool ServerLoop::Startup(const ServerConfig& config)
  {
    m_config = config;
    m_tickRate  = config.TickRate;
    m_tickDelta = 1.0f / m_tickRate;

    Telemetry::LogJson(LogLevel::Info, "ServerLoop starting", {
      {"port",       std::to_string(config.Port)},
      {"tickRate",   std::to_string(static_cast<int>(config.TickRate))},
      {"maxClients", std::to_string(config.MaxClients)},
      {"healthPort", std::to_string(config.HealthPort)},
    });

    if (!m_sessions.Startup(config.Port))
    {
      Telemetry::LogJson(LogLevel::Error, "Failed to start session manager");
      return false;
    }

    // Start health-check endpoint.
    if (!m_healthCheck.Start(config.HealthPort))
    {
      Neuron::Server::ServerLog("ServerLoop: Health check failed to start on port {}\n",
        config.HealthPort);
    }

    // Create a test zone with default entities.
    ZoneLoader::CreateTestZone(m_zone);

    Telemetry::LogJson(LogLevel::Info, "Startup complete", {
      {"entities", std::to_string(m_zone.GetEntityManager().ActiveCount())},
    });

    Neuron::Server::ServerLog("ServerLoop: Waiting for clients...\n");
    return true;
  }

  bool ServerLoop::Startup(uint16_t _port)
  {
    ServerConfig config;
    config.Port = _port;
    return Startup(config);
  }

  void ServerLoop::Shutdown()
  {
    m_running = false;
    m_healthCheck.Stop();
    m_sessions.Shutdown();
    Telemetry::LogJson(LogLevel::Info, "Shutdown complete");
    Neuron::Server::ServerLog("ServerLoop: Shutdown complete\n");
  }

  void ServerLoop::Run()
  {
    m_running = true;

    Neuron::Timer::Core::Startup();
    Neuron::Timer::Core::ResetElapsedTime();

    double accumulator = 0.0;

    while (m_running)
    {
      Neuron::Timer::Core::Update();
      double currentTime = Neuron::Timer::Core::GetTotalSeconds();
      double frameTime = static_cast<double>(Neuron::Timer::Core::GetElapsedSeconds());

      // Clamp frame time to prevent spiral-of-death
      if (frameTime > 0.25)
        frameTime = 0.25;

      accumulator += frameTime;

      // Poll network (always, regardless of simulation tick)
      m_sessions.Poll(currentTime);
      m_sessions.ResendReliable(currentTime);
      m_sessions.PruneTimedOut(currentTime);

      // Fixed-timestep simulation ticks
      while (accumulator >= m_tickDelta)
      {
        m_zone.Tick(m_tickDelta);
        accumulator -= m_tickDelta;
      }

      // Broadcast state to clients
      m_broadcaster.BroadcastState(currentTime);
      m_broadcaster.BroadcastShipStatus(currentTime);

      // Yield to avoid burning CPU when idle.
      double elapsed = static_cast<double>(Neuron::Timer::Core::GetElapsedSeconds());
      double sleepMs = (m_tickDelta - elapsed) * 1000.0;
      if (sleepMs > 1.0)
      {
        ::Sleep(static_cast<DWORD>(sleepMs));
      }
    }
  }
}
