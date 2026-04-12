#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace EarthRise
{
  // Password hashing and session token management using Windows BCrypt API.
  // Uses PBKDF2-SHA256 for password hashing with random salts.
  class AuthSystem
  {
  public:
    static constexpr uint32_t SALT_BYTES          = 16;
    static constexpr uint32_t HASH_BYTES          = 32;
    static constexpr uint32_t PBKDF2_ITERATIONS   = 100'000;
    static constexpr uint32_t SESSION_TOKEN_BYTES = 32;

    // Generate a cryptographically random salt (returned as hex string).
    [[nodiscard]] static std::string GenerateSalt();

    // Hash a password with the given salt using PBKDF2-SHA256.
    // Both salt and return value are hex-encoded.
    [[nodiscard]] static std::string HashPassword(std::string_view password,
                                                  std::string_view saltHex);

    // Verify a password against a stored hash and salt.
    [[nodiscard]] static bool VerifyPassword(std::string_view password,
                                             std::string_view saltHex,
                                             std::string_view hashHex);

    // Generate a cryptographically random session token (hex string).
    [[nodiscard]] static std::string GenerateSessionToken();

    // Hex encoding utilities.
    [[nodiscard]] static std::string BytesToHex(const uint8_t* data,
                                                size_t length);
    [[nodiscard]] static std::vector<uint8_t> HexToBytes(std::string_view hex);

  private:
    // Generate cryptographically random bytes.
    static bool GenerateRandomBytes(uint8_t* buffer, uint32_t length);
  };
}
