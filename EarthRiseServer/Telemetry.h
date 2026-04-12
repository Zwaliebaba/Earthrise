#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <initializer_list>
#include <utility>

namespace EarthRise
{
  enum class LogLevel
  {
    Info,
    Warning,
    Error,
  };

  // Structured logging for containerized deployment.
  // Outputs JSON-formatted log entries to stdout for container log drivers.
  class Telemetry
  {
  public:
    // Log a structured JSON message with optional key-value fields.
    static void LogJson(LogLevel level, std::string_view message,
      std::initializer_list<std::pair<std::string_view, std::string_view>> fields = {});

    // Log a plain-text message (for non-JSON log format).
    static void LogText(LogLevel level, std::string_view message);

    // Format a JSON log entry without printing it. Useful for testing.
    [[nodiscard]] static std::string FormatJsonEntry(
      LogLevel level, std::string_view message,
      std::initializer_list<std::pair<std::string_view, std::string_view>> fields = {});

    // Convert LogLevel to string.
    [[nodiscard]] static std::string_view LevelToString(LogLevel level) noexcept;

    // Get current UTC timestamp in ISO 8601 format.
    [[nodiscard]] static std::string GetTimestamp();

    // Escape a string for JSON output.
    [[nodiscard]] static std::string EscapeJson(std::string_view s);

    // Set a callback to capture log output (for testing).
    // Pass nullptr to reset to default (stdout).
    static void SetOutputCallback(std::function<void(std::string_view)> callback);
    static void ResetOutputCallback();

  private:
    static std::function<void(std::string_view)> s_outputCallback;
    static void Output(std::string_view text);
  };
}
