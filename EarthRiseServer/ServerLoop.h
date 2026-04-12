#pragma once

#include "Zone.h"
#include "ZoneLoader.h"
#include "StateBroadcaster.h"
#include "SessionManager.h"
#include "BandwidthManager.h"
#include "ServerConfig.h"
#include "HealthCheck.h"
#include "Telemetry.h"

namespace EarthRise
{
  // ServerLoop — fixed-timestep game simulation loop at 20 Hz.
  // Processes network input, ticks the zone, and broadcasts state.
  class ServerLoop
  {
  public:
    // Default tick rate: 20 Hz (50 ms per tick).
    static constexpr float TICK_RATE   = 20.0f;
    static constexpr float TICK_DELTA  = 1.0f / TICK_RATE;
    static constexpr uint16_t DEFAULT_PORT = 7777;

    ServerLoop();

    // Initialize from a ServerConfig: start socket, health check, test zone.
    bool Startup(const ServerConfig& config);

    // Legacy overload — uses default config with specified port.
    bool Startup(uint16_t _port = DEFAULT_PORT);

    // Shut down the server.
    void Shutdown();

    // Run the main loop (blocking). Returns when m_running is set to false.
    void Run();

    // Signal the loop to stop.
    void RequestStop() noexcept { m_running = false; }

    // Access the zone (for testing/injection).
    [[nodiscard]] Zone& GetZone() noexcept { return m_zone; }

  private:
    Zone                              m_zone;
    Neuron::Server::SessionManager    m_sessions;
    Neuron::Server::BandwidthManager  m_bandwidth;
    StateBroadcaster                  m_broadcaster;
    HealthCheck                       m_healthCheck;
    ServerConfig                      m_config;

    float m_tickRate  = TICK_RATE;
    float m_tickDelta = TICK_DELTA;
    bool  m_running   = false;
  };
}
