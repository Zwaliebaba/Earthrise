#include "pch.h"
#include "ServerLoop.h"

namespace EarthRise
{
  ServerLoop::ServerLoop()
    : m_broadcaster(m_zone, m_sessions, m_bandwidth)
  {
  }

  bool ServerLoop::Startup(uint16_t _port)
  {
    Neuron::DebugTrace("ServerLoop: Starting on port {}\n", _port);

    if (!m_sessions.Startup(_port))
    {
      Neuron::DebugTrace("ServerLoop: Failed to start session manager\n");
      return false;
    }

    // Create a test zone with default entities.
    ZoneLoader::CreateTestZone(m_zone);

    Neuron::DebugTrace("ServerLoop: Startup complete. Tick rate: {} Hz\n",
      static_cast<int>(TICK_RATE));
    return true;
  }

  void ServerLoop::Shutdown()
  {
    m_running = false;
    m_sessions.Shutdown();
    Neuron::DebugTrace("ServerLoop: Shutdown complete\n");
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
      while (accumulator >= TICK_DELTA)
      {
        m_zone.Tick(TICK_DELTA);
        accumulator -= TICK_DELTA;
      }

      // Broadcast state to clients
      m_broadcaster.BroadcastState(currentTime);

      // Yield to avoid burning CPU when idle.
      double elapsed = static_cast<double>(Neuron::Timer::Core::GetElapsedSeconds());
      double sleepMs = (TICK_DELTA - elapsed) * 1000.0;
      if (sleepMs > 1.0)
      {
        ::Sleep(static_cast<DWORD>(sleepMs));
      }
    }
  }
}
