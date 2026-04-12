#pragma once

namespace EarthRise
{
  // Server configuration loaded from environment variables and/or .env file.
  // Environment variables take precedence over .env file values.
  // All keys are prefixed with EARTHRISE_ (e.g. EARTHRISE_PORT).
  struct ServerConfig
  {
    uint16_t    Port               = 7777;
    float       TickRate           = 20.0f;
    uint32_t    MaxClients         = 100;
    uint16_t    HealthPort         = 8080;
    std::string DbConnectionString;
    std::string ZoneFile           = "GameData/Zones/default.zone";
    std::string LogFormat          = "json";

    // Load configuration: reads .env file (if present), then environment
    // variable overrides. Returns the fully resolved config.
    [[nodiscard]] static ServerConfig Load(std::string_view envFilePath = ".env");

    // Parse a single KEY=VALUE line from a .env file.
    // Returns false for blank lines, comments (#), or invalid syntax.
    [[nodiscard]] static bool ParseEnvLine(std::string_view line,
                                           std::string& outKey,
                                           std::string& outValue);

    // Apply a key-value pair to this config (case-insensitive key match).
    void ApplyKeyValue(std::string_view key, std::string_view value);
  };
}
