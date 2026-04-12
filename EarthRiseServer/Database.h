#pragma once

#include <string>
#include <cstdint>

namespace EarthRise
{
  // Result codes for database operations.
  enum class DatabaseResult
  {
    Ok,
    ConnectionFailed,
    QueryFailed,
    NotFound,
    DuplicateKey,
  };

  // Persistent account record.
  struct AccountRecord
  {
    uint64_t    Id           = 0;
    std::string Username;
    std::string PasswordHash;  // hex-encoded PBKDF2 output
    std::string Salt;          // hex-encoded random salt
    double      CreatedAt    = 0.0;
    double      LastLogin    = 0.0;
  };

  // Abstract database interface.
  // Concrete implementations: SqlDatabase (ODBC/MSSQL), MockDatabase (tests).
  class IDatabase
  {
  public:
    virtual ~IDatabase() = default;

    virtual DatabaseResult Connect(std::string_view connectionString) = 0;
    virtual void Disconnect() = 0;
    [[nodiscard]] virtual bool IsConnected() const = 0;

    // Account operations.
    virtual DatabaseResult CreateAccount(const AccountRecord& account) = 0;
    virtual DatabaseResult FindAccount(std::string_view username,
                                       AccountRecord& out) = 0;
    virtual DatabaseResult UpdateLastLogin(uint64_t accountId,
                                           double timestamp) = 0;
  };
}
