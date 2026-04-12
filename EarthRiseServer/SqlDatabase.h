#pragma once

#include "Database.h"

// Forward-declare ODBC types to avoid leaking sql.h into headers.
typedef void* SQLHENV;
typedef void* SQLHDBC;

namespace EarthRise
{
  // MSSQL implementation of IDatabase via ODBC.
  class SqlDatabase final : public IDatabase
  {
  public:
    SqlDatabase() = default;
    ~SqlDatabase() override { Disconnect(); }

    DatabaseResult Connect(std::string_view connectionString) override;
    void Disconnect() override;
    [[nodiscard]] bool IsConnected() const override { return m_connected; }

    DatabaseResult CreateAccount(const AccountRecord& account) override;
    DatabaseResult FindAccount(std::string_view username,
                               AccountRecord& out) override;
    DatabaseResult UpdateLastLogin(uint64_t accountId,
                                   double timestamp) override;

  private:
    SQLHENV m_hEnv  = nullptr;
    SQLHDBC m_hDbc  = nullptr;
    bool    m_connected = false;
  };
}
