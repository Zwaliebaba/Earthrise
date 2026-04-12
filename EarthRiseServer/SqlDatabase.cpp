#include "pch.h"
#include "SqlDatabase.h"

#include <sql.h>
#include <sqlext.h>

#pragma comment(lib, "odbc32")

namespace EarthRise
{
  DatabaseResult SqlDatabase::Connect(std::string_view connectionString)
  {
    if (m_connected)
      Disconnect();

    // Allocate ODBC environment handle.
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_hEnv);
    if (!SQL_SUCCEEDED(ret))
      return DatabaseResult::ConnectionFailed;

    SQLSetEnvAttr(m_hEnv, SQL_ATTR_ODBC_VERSION,
                  reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);

    // Allocate connection handle.
    ret = SQLAllocHandle(SQL_HANDLE_DBC, m_hEnv, &m_hDbc);
    if (!SQL_SUCCEEDED(ret))
    {
      SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
      m_hEnv = nullptr;
      return DatabaseResult::ConnectionFailed;
    }

    // Connect using the connection string.
    std::string connStr(connectionString);
    SQLCHAR outBuf[1024]{};
    SQLSMALLINT outLen = 0;

    ret = SQLDriverConnectA(
      m_hDbc, nullptr,
      reinterpret_cast<SQLCHAR*>(connStr.data()),
      static_cast<SQLSMALLINT>(connStr.size()),
      outBuf, sizeof(outBuf), &outLen,
      SQL_DRIVER_NOPROMPT);

    if (!SQL_SUCCEEDED(ret))
    {
      SQLFreeHandle(SQL_HANDLE_DBC, m_hDbc);
      SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
      m_hDbc = nullptr;
      m_hEnv = nullptr;
      return DatabaseResult::ConnectionFailed;
    }

    m_connected = true;
    return DatabaseResult::Ok;
  }

  void SqlDatabase::Disconnect()
  {
    if (m_hDbc)
    {
      if (m_connected)
        SQLDisconnect(m_hDbc);
      SQLFreeHandle(SQL_HANDLE_DBC, m_hDbc);
      m_hDbc = nullptr;
    }
    if (m_hEnv)
    {
      SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
      m_hEnv = nullptr;
    }
    m_connected = false;
  }

  DatabaseResult SqlDatabase::CreateAccount(const AccountRecord& account)
  {
    if (!m_connected)
      return DatabaseResult::ConnectionFailed;

    SQLHSTMT hStmt = nullptr;
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, m_hDbc, &hStmt);
    if (!SQL_SUCCEEDED(ret))
      return DatabaseResult::QueryFailed;

    const char* sql =
      "INSERT INTO Accounts (Username, PasswordHash, Salt) VALUES (?, ?, ?)";

    ret = SQLPrepareA(hStmt, reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql)),
                      SQL_NTS);
    if (!SQL_SUCCEEDED(ret))
    {
      SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
      return DatabaseResult::QueryFailed;
    }

    // Bind parameters.
    std::string username = account.Username;
    std::string hash     = account.PasswordHash;
    std::string salt     = account.Salt;

    SQLLEN usernameLen = static_cast<SQLLEN>(username.size());
    SQLLEN hashLen     = static_cast<SQLLEN>(hash.size());
    SQLLEN saltLen     = static_cast<SQLLEN>(salt.size());

    SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                     64, 0, username.data(), 0, &usernameLen);
    SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                     128, 0, hash.data(), 0, &hashLen);
    SQLBindParameter(hStmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                     64, 0, salt.data(), 0, &saltLen);

    ret = SQLExecute(hStmt);

    DatabaseResult result = DatabaseResult::Ok;
    if (!SQL_SUCCEEDED(ret))
    {
      // Check for unique constraint violation (SQL State 23000).
      SQLCHAR sqlState[6]{};
      SQLGetDiagFieldA(SQL_HANDLE_STMT, hStmt, 1, SQL_DIAG_SQLSTATE,
                       sqlState, sizeof(sqlState), nullptr);
      if (std::strcmp(reinterpret_cast<char*>(sqlState), "23000") == 0)
        result = DatabaseResult::DuplicateKey;
      else
        result = DatabaseResult::QueryFailed;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return result;
  }

  DatabaseResult SqlDatabase::FindAccount(std::string_view username,
                                          AccountRecord& out)
  {
    if (!m_connected)
      return DatabaseResult::ConnectionFailed;

    SQLHSTMT hStmt = nullptr;
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, m_hDbc, &hStmt);
    if (!SQL_SUCCEEDED(ret))
      return DatabaseResult::QueryFailed;

    const char* sql =
      "SELECT Id, Username, PasswordHash, Salt FROM Accounts WHERE Username = ?";

    ret = SQLPrepareA(hStmt, reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql)),
                      SQL_NTS);
    if (!SQL_SUCCEEDED(ret))
    {
      SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
      return DatabaseResult::QueryFailed;
    }

    std::string user(username);
    SQLLEN userLen = static_cast<SQLLEN>(user.size());
    SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                     64, 0, user.data(), 0, &userLen);

    ret = SQLExecute(hStmt);
    if (!SQL_SUCCEEDED(ret))
    {
      SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
      return DatabaseResult::QueryFailed;
    }

    ret = SQLFetch(hStmt);
    if (ret == SQL_NO_DATA)
    {
      SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
      return DatabaseResult::NotFound;
    }

    if (!SQL_SUCCEEDED(ret))
    {
      SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
      return DatabaseResult::QueryFailed;
    }

    // Extract columns.
    SQLBIGINT id = 0;
    char usernameBuf[65]{};
    char hashBuf[129]{};
    char saltBuf[65]{};
    SQLLEN ind = 0;

    SQLGetData(hStmt, 1, SQL_C_SBIGINT, &id, sizeof(id), &ind);
    SQLGetData(hStmt, 2, SQL_C_CHAR, usernameBuf, sizeof(usernameBuf), &ind);
    SQLGetData(hStmt, 3, SQL_C_CHAR, hashBuf, sizeof(hashBuf), &ind);
    SQLGetData(hStmt, 4, SQL_C_CHAR, saltBuf, sizeof(saltBuf), &ind);

    out.Id           = static_cast<uint64_t>(id);
    out.Username     = usernameBuf;
    out.PasswordHash = hashBuf;
    out.Salt         = saltBuf;

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return DatabaseResult::Ok;
  }

  DatabaseResult SqlDatabase::UpdateLastLogin(uint64_t accountId,
                                              double timestamp)
  {
    if (!m_connected)
      return DatabaseResult::ConnectionFailed;

    SQLHSTMT hStmt = nullptr;
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, m_hDbc, &hStmt);
    if (!SQL_SUCCEEDED(ret))
      return DatabaseResult::QueryFailed;

    const char* sql =
      "UPDATE Accounts SET LastLogin = GETUTCDATE() WHERE Id = ?";

    ret = SQLPrepareA(hStmt, reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql)),
                      SQL_NTS);
    if (!SQL_SUCCEEDED(ret))
    {
      SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
      return DatabaseResult::QueryFailed;
    }

    SQLBIGINT sqlId = static_cast<SQLBIGINT>(accountId);
    SQLLEN idLen = sizeof(sqlId);
    SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT,
                     0, 0, &sqlId, 0, &idLen);

    ret = SQLExecute(hStmt);
    DatabaseResult result = SQL_SUCCEEDED(ret)
      ? DatabaseResult::Ok
      : DatabaseResult::QueryFailed;

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return result;
  }
}
