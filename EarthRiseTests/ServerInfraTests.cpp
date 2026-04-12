#include "pch.h"

#include "ServerConfig.h"
#include "AuthSystem.h"
#include "Telemetry.h"
#include "Database.h"
#include "HealthCheck.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace EarthRiseTests::Infrastructure
{
  // ========================================================================
  // MockDatabase — in-memory IDatabase for unit testing.
  // ========================================================================
  class MockDatabase final : public EarthRise::IDatabase
  {
  public:
    EarthRise::DatabaseResult Connect(std::string_view) override
    {
      m_connected = true;
      return EarthRise::DatabaseResult::Ok;
    }

    void Disconnect() override { m_connected = false; }
    [[nodiscard]] bool IsConnected() const override { return m_connected; }

    EarthRise::DatabaseResult CreateAccount(
      const EarthRise::AccountRecord& account) override
    {
      for (const auto& a : m_accounts)
      {
        if (a.Username == account.Username)
          return EarthRise::DatabaseResult::DuplicateKey;
      }

      auto& stored     = m_accounts.emplace_back(account);
      stored.Id        = m_nextId++;
      stored.CreatedAt = 1000.0;
      return EarthRise::DatabaseResult::Ok;
    }

    EarthRise::DatabaseResult FindAccount(
      std::string_view username, EarthRise::AccountRecord& out) override
    {
      for (const auto& a : m_accounts)
      {
        if (a.Username == username)
        {
          out = a;
          return EarthRise::DatabaseResult::Ok;
        }
      }
      return EarthRise::DatabaseResult::NotFound;
    }

    EarthRise::DatabaseResult UpdateLastLogin(
      uint64_t accountId, double timestamp) override
    {
      for (auto& a : m_accounts)
      {
        if (a.Id == accountId)
        {
          a.LastLogin = timestamp;
          return EarthRise::DatabaseResult::Ok;
        }
      }
      return EarthRise::DatabaseResult::NotFound;
    }

    [[nodiscard]] size_t Count() const noexcept { return m_accounts.size(); }

  private:
    std::vector<EarthRise::AccountRecord> m_accounts;
    uint64_t m_nextId = 1;
    bool m_connected  = false;
  };

  // ========================================================================
  // ServerConfigTests
  // ========================================================================
  TEST_CLASS(ServerConfigTests)
  {
  public:
    TEST_METHOD(DefaultPort)
    {
      EarthRise::ServerConfig config;
      Assert::AreEqual(static_cast<uint16_t>(7777), config.Port);
    }

    TEST_METHOD(DefaultTickRate)
    {
      EarthRise::ServerConfig config;
      Assert::AreEqual(20.0f, config.TickRate);
    }

    TEST_METHOD(DefaultMaxClients)
    {
      EarthRise::ServerConfig config;
      Assert::AreEqual(100u, config.MaxClients);
    }

    TEST_METHOD(DefaultHealthPort)
    {
      EarthRise::ServerConfig config;
      Assert::AreEqual(static_cast<uint16_t>(8080), config.HealthPort);
    }

    TEST_METHOD(DefaultLogFormat)
    {
      EarthRise::ServerConfig config;
      Assert::AreEqual(std::string("json"), config.LogFormat);
    }

    TEST_METHOD(ParseEnvLineSimple)
    {
      std::string key, value;
      bool ok = EarthRise::ServerConfig::ParseEnvLine("EARTHRISE_PORT=9999",
                                                       key, value);
      Assert::IsTrue(ok);
      Assert::AreEqual(std::string("EARTHRISE_PORT"), key);
      Assert::AreEqual(std::string("9999"), value);
    }

    TEST_METHOD(ParseEnvLineComment)
    {
      std::string key, value;
      bool ok = EarthRise::ServerConfig::ParseEnvLine("# this is a comment",
                                                       key, value);
      Assert::IsFalse(ok);
    }

    TEST_METHOD(ParseEnvLineBlank)
    {
      std::string key, value;
      bool ok = EarthRise::ServerConfig::ParseEnvLine("", key, value);
      Assert::IsFalse(ok);
    }

    TEST_METHOD(ParseEnvLineQuoted)
    {
      std::string key, value;
      bool ok = EarthRise::ServerConfig::ParseEnvLine(
        "EARTHRISE_DB_CONNECTION_STRING=\"Driver={SQL Server};Server=localhost\"",
        key, value);
      Assert::IsTrue(ok);
      Assert::AreEqual(std::string("EARTHRISE_DB_CONNECTION_STRING"), key);
      Assert::AreEqual(std::string("Driver={SQL Server};Server=localhost"), value);
    }

    TEST_METHOD(ApplyKeyValuePort)
    {
      EarthRise::ServerConfig config;
      config.ApplyKeyValue("EARTHRISE_PORT", "9999");
      Assert::AreEqual(static_cast<uint16_t>(9999), config.Port);
    }

    TEST_METHOD(ApplyKeyValueTickRate)
    {
      EarthRise::ServerConfig config;
      config.ApplyKeyValue("EARTHRISE_TICK_RATE", "60");
      Assert::AreEqual(60.0f, config.TickRate);
    }

    TEST_METHOD(ApplyKeyValueCaseInsensitive)
    {
      EarthRise::ServerConfig config;
      config.ApplyKeyValue("earthrise_port", "8888");
      Assert::AreEqual(static_cast<uint16_t>(8888), config.Port);
    }

    TEST_METHOD(ApplyKeyValueInvalidPortIgnored)
    {
      EarthRise::ServerConfig config;
      config.ApplyKeyValue("EARTHRISE_PORT", "0");
      Assert::AreEqual(static_cast<uint16_t>(7777), config.Port);
    }

    TEST_METHOD(ParseEnvLineWhitespace)
    {
      std::string key, value;
      bool ok = EarthRise::ServerConfig::ParseEnvLine(
        "  EARTHRISE_PORT  =  8080  ", key, value);
      Assert::IsTrue(ok);
      Assert::AreEqual(std::string("EARTHRISE_PORT"), key);
      Assert::AreEqual(std::string("8080"), value);
    }
  };

  // ========================================================================
  // AuthSystemTests
  // ========================================================================
  TEST_CLASS(AuthSystemTests)
  {
  public:
    TEST_METHOD(GenerateSaltNonEmpty)
    {
      auto salt = EarthRise::AuthSystem::GenerateSalt();
      Assert::IsFalse(salt.empty());
    }

    TEST_METHOD(GenerateSaltCorrectLength)
    {
      auto salt = EarthRise::AuthSystem::GenerateSalt();
      // 16 bytes → 32 hex characters.
      Assert::AreEqual(static_cast<size_t>(32), salt.size());
    }

    TEST_METHOD(GenerateSaltUnique)
    {
      auto salt1 = EarthRise::AuthSystem::GenerateSalt();
      auto salt2 = EarthRise::AuthSystem::GenerateSalt();
      Assert::AreNotEqual(salt1, salt2);
    }

    TEST_METHOD(HashPasswordNonEmpty)
    {
      auto salt = EarthRise::AuthSystem::GenerateSalt();
      auto hash = EarthRise::AuthSystem::HashPassword("password123", salt);
      Assert::IsFalse(hash.empty());
    }

    TEST_METHOD(HashPasswordCorrectLength)
    {
      auto salt = EarthRise::AuthSystem::GenerateSalt();
      auto hash = EarthRise::AuthSystem::HashPassword("password123", salt);
      // 32 bytes → 64 hex characters.
      Assert::AreEqual(static_cast<size_t>(64), hash.size());
    }

    TEST_METHOD(HashPasswordDeterministic)
    {
      auto salt  = EarthRise::AuthSystem::GenerateSalt();
      auto hash1 = EarthRise::AuthSystem::HashPassword("password123", salt);
      auto hash2 = EarthRise::AuthSystem::HashPassword("password123", salt);
      Assert::AreEqual(hash1, hash2);
    }

    TEST_METHOD(HashPasswordDifferentSalts)
    {
      auto salt1 = EarthRise::AuthSystem::GenerateSalt();
      auto salt2 = EarthRise::AuthSystem::GenerateSalt();
      auto hash1 = EarthRise::AuthSystem::HashPassword("password123", salt1);
      auto hash2 = EarthRise::AuthSystem::HashPassword("password123", salt2);
      Assert::AreNotEqual(hash1, hash2);
    }

    TEST_METHOD(VerifyPasswordCorrect)
    {
      auto salt = EarthRise::AuthSystem::GenerateSalt();
      auto hash = EarthRise::AuthSystem::HashPassword("mySecret!", salt);
      Assert::IsTrue(
        EarthRise::AuthSystem::VerifyPassword("mySecret!", salt, hash));
    }

    TEST_METHOD(VerifyPasswordWrong)
    {
      auto salt = EarthRise::AuthSystem::GenerateSalt();
      auto hash = EarthRise::AuthSystem::HashPassword("mySecret!", salt);
      Assert::IsFalse(
        EarthRise::AuthSystem::VerifyPassword("wrongPass", salt, hash));
    }

    TEST_METHOD(GenerateSessionTokenNonEmpty)
    {
      auto token = EarthRise::AuthSystem::GenerateSessionToken();
      Assert::IsFalse(token.empty());
    }

    TEST_METHOD(GenerateSessionTokenCorrectLength)
    {
      auto token = EarthRise::AuthSystem::GenerateSessionToken();
      // 32 bytes → 64 hex characters.
      Assert::AreEqual(static_cast<size_t>(64), token.size());
    }

    TEST_METHOD(GenerateSessionTokenUnique)
    {
      auto t1 = EarthRise::AuthSystem::GenerateSessionToken();
      auto t2 = EarthRise::AuthSystem::GenerateSessionToken();
      Assert::AreNotEqual(t1, t2);
    }

    TEST_METHOD(HexRoundTrip)
    {
      uint8_t data[] = {0x00, 0x01, 0xAB, 0xCD, 0xEF, 0xFF};
      auto hex = EarthRise::AuthSystem::BytesToHex(data, sizeof(data));
      Assert::AreEqual(std::string("0001abcdefff"), hex);

      auto bytes = EarthRise::AuthSystem::HexToBytes(hex);
      Assert::AreEqual(sizeof(data), bytes.size());
      for (size_t i = 0; i < sizeof(data); ++i)
        Assert::AreEqual(data[i], bytes[i]);
    }
  };

  // ========================================================================
  // TelemetryTests
  // ========================================================================
  TEST_CLASS(TelemetryTests)
  {
  public:
    TEST_METHOD(FormatJsonHasTimestamp)
    {
      auto json = EarthRise::Telemetry::FormatJsonEntry(
        EarthRise::LogLevel::Info, "test");
      Assert::IsTrue(json.find("\"ts\":") != std::string::npos);
    }

    TEST_METHOD(FormatJsonHasLevel)
    {
      auto json = EarthRise::Telemetry::FormatJsonEntry(
        EarthRise::LogLevel::Info, "test");
      Assert::IsTrue(json.find("\"level\":\"info\"") != std::string::npos);
    }

    TEST_METHOD(FormatJsonHasMessage)
    {
      auto json = EarthRise::Telemetry::FormatJsonEntry(
        EarthRise::LogLevel::Info, "hello world");
      Assert::IsTrue(json.find("\"msg\":\"hello world\"") != std::string::npos);
    }

    TEST_METHOD(FormatJsonExtraFields)
    {
      auto json = EarthRise::Telemetry::FormatJsonEntry(
        EarthRise::LogLevel::Info, "msg", {{"port", "7777"}, {"zone", "alpha"}});
      Assert::IsTrue(json.find("\"port\":\"7777\"") != std::string::npos);
      Assert::IsTrue(json.find("\"zone\":\"alpha\"") != std::string::npos);
    }

    TEST_METHOD(FormatJsonEscapesQuotes)
    {
      auto json = EarthRise::Telemetry::FormatJsonEntry(
        EarthRise::LogLevel::Info, "say \"hello\"");
      Assert::IsTrue(json.find("say \\\"hello\\\"") != std::string::npos);
    }

    TEST_METHOD(FormatJsonEscapesNewlines)
    {
      auto json = EarthRise::Telemetry::FormatJsonEntry(
        EarthRise::LogLevel::Info, "line1\nline2");
      Assert::IsTrue(json.find("line1\\nline2") != std::string::npos);
    }

    TEST_METHOD(LevelToStringValues)
    {
      Assert::AreEqual(std::string_view("info"),
        EarthRise::Telemetry::LevelToString(EarthRise::LogLevel::Info));
      Assert::AreEqual(std::string_view("warning"),
        EarthRise::Telemetry::LevelToString(EarthRise::LogLevel::Warning));
      Assert::AreEqual(std::string_view("error"),
        EarthRise::Telemetry::LevelToString(EarthRise::LogLevel::Error));
    }

    TEST_METHOD(GetTimestampFormat)
    {
      auto ts = EarthRise::Telemetry::GetTimestamp();
      // ISO 8601: "2026-04-12T10:30:00.123Z" — 24 characters.
      Assert::AreEqual(static_cast<size_t>(24), ts.size());
      Assert::AreEqual('T', ts[10]);
      Assert::AreEqual('Z', ts[23]);
    }

    TEST_METHOD(EscapeJsonControlChars)
    {
      auto escaped = EarthRise::Telemetry::EscapeJson("tab\there");
      Assert::AreEqual(std::string("tab\\there"), escaped);
    }

    TEST_METHOD(OutputCallbackCaptures)
    {
      std::string captured;
      EarthRise::Telemetry::SetOutputCallback(
        [&](std::string_view s) { captured = std::string(s); });

      EarthRise::Telemetry::LogJson(EarthRise::LogLevel::Info, "test capture");

      EarthRise::Telemetry::ResetOutputCallback();

      Assert::IsFalse(captured.empty());
      Assert::IsTrue(captured.find("test capture") != std::string::npos);
    }
  };

  // ========================================================================
  // DatabaseMockTests
  // ========================================================================
  TEST_CLASS(DatabaseMockTests)
  {
  public:
    TEST_METHOD(ConnectSucceeds)
    {
      MockDatabase db;
      Assert::IsFalse(db.IsConnected());
      auto result = db.Connect("test");
      Assert::IsTrue(result == EarthRise::DatabaseResult::Ok);
      Assert::IsTrue(db.IsConnected());
    }

    TEST_METHOD(DisconnectSetsFlag)
    {
      MockDatabase db;
      db.Connect("test");
      db.Disconnect();
      Assert::IsFalse(db.IsConnected());
    }

    TEST_METHOD(CreateAccountStoresRecord)
    {
      MockDatabase db;
      db.Connect("test");

      EarthRise::AccountRecord acct;
      acct.Username     = "player1";
      acct.PasswordHash = "abc123";
      acct.Salt         = "salt456";

      auto result = db.CreateAccount(acct);
      Assert::IsTrue(result == EarthRise::DatabaseResult::Ok);
      Assert::AreEqual(static_cast<size_t>(1), db.Count());
    }

    TEST_METHOD(FindAccountRetrievesRecord)
    {
      MockDatabase db;
      db.Connect("test");

      EarthRise::AccountRecord acct;
      acct.Username     = "player1";
      acct.PasswordHash = "hash";
      acct.Salt         = "salt";
      db.CreateAccount(acct);

      EarthRise::AccountRecord found;
      auto result = db.FindAccount("player1", found);
      Assert::IsTrue(result == EarthRise::DatabaseResult::Ok);
      Assert::AreEqual(std::string("player1"), found.Username);
      Assert::AreEqual(std::string("hash"), found.PasswordHash);
    }

    TEST_METHOD(FindAccountNotFound)
    {
      MockDatabase db;
      db.Connect("test");

      EarthRise::AccountRecord found;
      auto result = db.FindAccount("nonexistent", found);
      Assert::IsTrue(result == EarthRise::DatabaseResult::NotFound);
    }

    TEST_METHOD(CreateAccountDuplicateKey)
    {
      MockDatabase db;
      db.Connect("test");

      EarthRise::AccountRecord acct;
      acct.Username     = "player1";
      acct.PasswordHash = "hash";
      acct.Salt         = "salt";
      db.CreateAccount(acct);

      auto result = db.CreateAccount(acct);
      Assert::IsTrue(result == EarthRise::DatabaseResult::DuplicateKey);
    }

    TEST_METHOD(UpdateLastLoginModifiesRecord)
    {
      MockDatabase db;
      db.Connect("test");

      EarthRise::AccountRecord acct;
      acct.Username     = "player1";
      acct.PasswordHash = "hash";
      acct.Salt         = "salt";
      db.CreateAccount(acct);

      EarthRise::AccountRecord found;
      db.FindAccount("player1", found);

      auto result = db.UpdateLastLogin(found.Id, 99999.0);
      Assert::IsTrue(result == EarthRise::DatabaseResult::Ok);

      EarthRise::AccountRecord updated;
      db.FindAccount("player1", updated);
      Assert::AreEqual(99999.0, updated.LastLogin);
    }

    TEST_METHOD(InterfacePolymorphism)
    {
      MockDatabase mock;
      EarthRise::IDatabase* db = &mock;

      auto result = db->Connect("test");
      Assert::IsTrue(result == EarthRise::DatabaseResult::Ok);
      Assert::IsTrue(db->IsConnected());

      db->Disconnect();
      Assert::IsFalse(db->IsConnected());
    }
  };

  // ========================================================================
  // HealthCheckTests
  // ========================================================================
  TEST_CLASS(HealthCheckTests)
  {
  public:
    TEST_CLASS_INITIALIZE(InitWinsock)
    {
      WSADATA wsa;
      WSAStartup(MAKEWORD(2, 2), &wsa);
    }

    TEST_CLASS_CLEANUP(CleanupWinsock)
    {
      WSACleanup();
    }

    TEST_METHOD(NotRunningByDefault)
    {
      EarthRise::HealthCheck hc;
      Assert::IsFalse(hc.IsRunning());
      Assert::AreEqual(static_cast<uint16_t>(0), hc.GetPort());
    }

    TEST_METHOD(StartSetsRunning)
    {
      EarthRise::HealthCheck hc;
      bool ok = hc.Start(0); // auto-assign port
      Assert::IsTrue(ok);
      Assert::IsTrue(hc.IsRunning());
      Assert::IsTrue(hc.GetPort() > 0);
      hc.Stop();
    }

    TEST_METHOD(StopClearsRunning)
    {
      EarthRise::HealthCheck hc;
      hc.Start(0);
      hc.Stop();
      Assert::IsFalse(hc.IsRunning());
    }

    TEST_METHOD(RespondsToTcpConnection)
    {
      EarthRise::HealthCheck hc;
      Assert::IsTrue(hc.Start(0));
      uint16_t port = hc.GetPort();

      // Connect via TCP to localhost.
      SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      Assert::IsTrue(sock != INVALID_SOCKET);

      sockaddr_in addr{};
      addr.sin_family      = AF_INET;
      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      addr.sin_port        = htons(port);

      int connectResult = connect(sock, reinterpret_cast<sockaddr*>(&addr),
                                  sizeof(addr));
      Assert::AreEqual(0, connectResult);

      char buf[512]{};
      int received = recv(sock, buf, sizeof(buf) - 1, 0);
      Assert::IsTrue(received > 0);
      Assert::IsTrue(std::strstr(buf, "200 OK") != nullptr);

      closesocket(sock);
      hc.Stop();
    }
  };
}
