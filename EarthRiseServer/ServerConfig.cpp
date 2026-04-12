#include "pch.h"
#include "ServerConfig.h"
#include <fstream>

namespace EarthRise
{
  bool ServerConfig::ParseEnvLine(std::string_view line,
                                  std::string& outKey,
                                  std::string& outValue)
  {
    // Trim leading whitespace.
    auto start = line.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos)
      return false;

    line = line.substr(start);

    // Skip comments.
    if (line.front() == '#')
      return false;

    // Find the '=' separator.
    auto eq = line.find('=');
    if (eq == std::string_view::npos || eq == 0)
      return false;

    // Extract key (trim trailing whitespace).
    auto keyView = line.substr(0, eq);
    auto keyEnd = keyView.find_last_not_of(" \t");
    if (keyEnd == std::string_view::npos)
      return false;

    outKey = std::string(keyView.substr(0, keyEnd + 1));

    // Extract value (trim whitespace, strip optional quotes).
    auto valView = line.substr(eq + 1);
    auto valStart = valView.find_first_not_of(" \t");
    if (valStart == std::string_view::npos)
    {
      outValue.clear();
      return true;
    }

    valView = valView.substr(valStart);

    // Trim trailing whitespace and carriage returns.
    auto valEnd = valView.find_last_not_of(" \t\r\n");
    if (valEnd != std::string_view::npos)
      valView = valView.substr(0, valEnd + 1);

    // Strip surrounding quotes (single or double).
    if (valView.size() >= 2 &&
        ((valView.front() == '"' && valView.back() == '"') ||
         (valView.front() == '\'' && valView.back() == '\'')))
    {
      valView = valView.substr(1, valView.size() - 2);
    }

    outValue = std::string(valView);
    return true;
  }

  void ServerConfig::ApplyKeyValue(std::string_view key, std::string_view value)
  {
    // Case-insensitive comparison via uppercase conversion.
    std::string upper(key);
    for (auto& c : upper)
      c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (upper == "EARTHRISE_PORT")
    {
      int v = std::atoi(std::string(value).c_str());
      if (v > 0 && v <= 65535)
        Port = static_cast<uint16_t>(v);
    }
    else if (upper == "EARTHRISE_TICK_RATE")
    {
      float v = static_cast<float>(std::atof(std::string(value).c_str()));
      if (v > 0.0f && v <= 1000.0f)
        TickRate = v;
    }
    else if (upper == "EARTHRISE_MAX_CLIENTS")
    {
      int v = std::atoi(std::string(value).c_str());
      if (v > 0)
        MaxClients = static_cast<uint32_t>(v);
    }
    else if (upper == "EARTHRISE_HEALTH_PORT")
    {
      int v = std::atoi(std::string(value).c_str());
      if (v > 0 && v <= 65535)
        HealthPort = static_cast<uint16_t>(v);
    }
    else if (upper == "EARTHRISE_DB_CONNECTION_STRING")
    {
      DbConnectionString = std::string(value);
    }
    else if (upper == "EARTHRISE_ZONE_FILE")
    {
      ZoneFile = std::string(value);
    }
    else if (upper == "EARTHRISE_LOG_FORMAT")
    {
      LogFormat = std::string(value);
    }
    else if (upper == "EARTHRISE_UNIVERSE_SEED")
    {
      uint64_t v = std::strtoull(std::string(value).c_str(), nullptr, 10);
      UniverseSeed = v;
    }
    else if (upper == "EARTHRISE_DELTA_SAVE_PATH")
    {
      DeltaSavePath = std::string(value);
    }
    else if (upper == "EARTHRISE_DELTA_SAVE_INTERVAL")
    {
      float v = static_cast<float>(std::atof(std::string(value).c_str()));
      if (v > 0.0f)
        DeltaSaveIntervalSec = v;
    }
  }

  ServerConfig ServerConfig::Load(std::string_view envFilePath)
  {
    ServerConfig config;

    // 1. Read .env file (if it exists).
    std::string envPath(envFilePath);
    std::ifstream envFile(envPath);
    if (envFile.is_open())
    {
      std::string line;
      while (std::getline(envFile, line))
      {
        std::string key, value;
        if (ParseEnvLine(line, key, value))
          config.ApplyKeyValue(key, value);
      }
    }

    // 2. Environment variable overrides (take precedence).
    constexpr const char* keys[] = {
      "EARTHRISE_PORT",
      "EARTHRISE_TICK_RATE",
      "EARTHRISE_MAX_CLIENTS",
      "EARTHRISE_HEALTH_PORT",
      "EARTHRISE_DB_CONNECTION_STRING",
      "EARTHRISE_ZONE_FILE",
      "EARTHRISE_LOG_FORMAT",
      "EARTHRISE_UNIVERSE_SEED",
      "EARTHRISE_DELTA_SAVE_PATH",
      "EARTHRISE_DELTA_SAVE_INTERVAL",
    };

    for (const char* envKey : keys)
    {
      char buf[1024]{};
      DWORD len = GetEnvironmentVariableA(envKey, buf, sizeof(buf));
      if (len > 0 && len < sizeof(buf))
        config.ApplyKeyValue(envKey, std::string_view(buf, len));
    }

    return config;
  }
}
