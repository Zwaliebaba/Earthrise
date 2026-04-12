#include "pch.h"
#include "AuthSystem.h"

#include <bcrypt.h>
#pragma comment(lib, "bcrypt")

namespace EarthRise
{
  bool AuthSystem::GenerateRandomBytes(uint8_t* buffer, uint32_t length)
  {
    NTSTATUS status = BCryptGenRandom(
      nullptr, buffer, length, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return BCRYPT_SUCCESS(status);
  }

  std::string AuthSystem::GenerateSalt()
  {
    uint8_t salt[SALT_BYTES]{};
    if (!GenerateRandomBytes(salt, SALT_BYTES))
      return {};

    return BytesToHex(salt, SALT_BYTES);
  }

  std::string AuthSystem::HashPassword(std::string_view password,
                                       std::string_view saltHex)
  {
    auto saltBytes = HexToBytes(saltHex);
    if (saltBytes.empty())
      return {};

    // Open HMAC-SHA256 algorithm provider for PBKDF2.
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(
      &hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);

    if (!BCRYPT_SUCCESS(status))
      return {};

    uint8_t derived[HASH_BYTES]{};
    status = BCryptDeriveKeyPBKDF2(
      hAlg,
      reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())),
      static_cast<ULONG>(password.size()),
      saltBytes.data(),
      static_cast<ULONG>(saltBytes.size()),
      PBKDF2_ITERATIONS,
      derived,
      HASH_BYTES,
      0);

    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (!BCRYPT_SUCCESS(status))
      return {};

    return BytesToHex(derived, HASH_BYTES);
  }

  bool AuthSystem::VerifyPassword(std::string_view password,
                                  std::string_view saltHex,
                                  std::string_view hashHex)
  {
    std::string computed = HashPassword(password, saltHex);
    if (computed.empty() || computed.size() != hashHex.size())
      return false;

    // Constant-time comparison to prevent timing attacks.
    volatile uint8_t diff = 0;
    for (size_t i = 0; i < computed.size(); ++i)
      diff |= static_cast<uint8_t>(computed[i] ^ hashHex[i]);

    return diff == 0;
  }

  std::string AuthSystem::GenerateSessionToken()
  {
    uint8_t token[SESSION_TOKEN_BYTES]{};
    if (!GenerateRandomBytes(token, SESSION_TOKEN_BYTES))
      return {};

    return BytesToHex(token, SESSION_TOKEN_BYTES);
  }

  std::string AuthSystem::BytesToHex(const uint8_t* data, size_t length)
  {
    static constexpr char hexChars[] = "0123456789abcdef";

    std::string result;
    result.reserve(length * 2);

    for (size_t i = 0; i < length; ++i)
    {
      result += hexChars[(data[i] >> 4) & 0x0F];
      result += hexChars[data[i] & 0x0F];
    }

    return result;
  }

  std::vector<uint8_t> AuthSystem::HexToBytes(std::string_view hex)
  {
    if (hex.size() % 2 != 0)
      return {};

    auto hexVal = [](char c) -> int
    {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
    };

    std::vector<uint8_t> result;
    result.reserve(hex.size() / 2);

    for (size_t i = 0; i < hex.size(); i += 2)
    {
      int hi = hexVal(hex[i]);
      int lo = hexVal(hex[i + 1]);
      if (hi < 0 || lo < 0)
        return {};

      result.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }

    return result;
  }
}
