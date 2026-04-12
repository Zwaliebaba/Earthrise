#include "pch.h"
#include "Telemetry.h"
#include <chrono>

namespace EarthRise
{
  std::function<void(std::string_view)> Telemetry::s_outputCallback;

  void Telemetry::LogJson(LogLevel level, std::string_view message,
    std::initializer_list<std::pair<std::string_view, std::string_view>> fields)
  {
    std::string entry = FormatJsonEntry(level, message, fields);
    entry += '\n';
    Output(entry);
  }

  void Telemetry::LogText(LogLevel level, std::string_view message)
  {
    std::string entry = std::format("[{}] {}: {}\n",
      GetTimestamp(), LevelToString(level), message);
    Output(entry);
  }

  std::string Telemetry::FormatJsonEntry(
    LogLevel level, std::string_view message,
    std::initializer_list<std::pair<std::string_view, std::string_view>> fields)
  {
    std::string json;
    json.reserve(256);

    json += "{\"ts\":\"";
    json += GetTimestamp();
    json += "\",\"level\":\"";
    json += LevelToString(level);
    json += "\",\"msg\":\"";
    json += EscapeJson(message);
    json += '"';

    for (const auto& [key, value] : fields)
    {
      json += ",\"";
      json += EscapeJson(key);
      json += "\":\"";
      json += EscapeJson(value);
      json += '"';
    }

    json += '}';
    return json;
  }

  std::string_view Telemetry::LevelToString(LogLevel level) noexcept
  {
    switch (level)
    {
    case LogLevel::Info:    return "info";
    case LogLevel::Warning: return "warning";
    case LogLevel::Error:   return "error";
    default:                return "unknown";
    }
  }

  std::string Telemetry::GetTimestamp()
  {
    auto now = std::chrono::system_clock::now();
    auto tt  = std::chrono::system_clock::to_time_t(now);
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()) % 1000;

    std::tm utc{};
    gmtime_s(&utc, &tt);

    return std::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}Z",
      utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
      utc.tm_hour, utc.tm_min, utc.tm_sec,
      static_cast<int>(ms.count()));
  }

  std::string Telemetry::EscapeJson(std::string_view s)
  {
    std::string result;
    result.reserve(s.size());

    for (char c : s)
    {
      switch (c)
      {
      case '"':  result += "\\\""; break;
      case '\\': result += "\\\\"; break;
      case '\n': result += "\\n";  break;
      case '\r': result += "\\r";  break;
      case '\t': result += "\\t";  break;
      default:
        if (static_cast<unsigned char>(c) < 0x20)
          result += std::format("\\u{:04x}", static_cast<uint32_t>(c));
        else
          result += c;
        break;
      }
    }

    return result;
  }

  void Telemetry::SetOutputCallback(std::function<void(std::string_view)> callback)
  {
    s_outputCallback = std::move(callback);
  }

  void Telemetry::ResetOutputCallback()
  {
    s_outputCallback = nullptr;
  }

  void Telemetry::Output(std::string_view text)
  {
    if (s_outputCallback)
    {
      s_outputCallback(text);
    }
    else
    {
      std::printf("%.*s", static_cast<int>(text.size()), text.data());
      std::fflush(stdout);
    }
  }
}
