#pragma once

// ---------------------------------------------------------------------------
// SerializationBase.h — CRTP serialization base for cursor-based binary I/O
//
// Provides Read<T>() / Write<T>() over a byte_buffer_t (std::vector<uint8_t>).
// Shared by:
//   - DataReader / DataWriter      — network packets (fixed DATALOAD_SIZE)
//   - BinaryDataReader / Writer    — disk files (unlimited size)
//
// BinaryDataReader / BinaryDataWriter wrap a byte_buffer_t with a cursor,
// enabling structured deserialization of arbitrary-length data loaded via
// BinaryFile::ReadFile / WriteFile.  Not constrained by DATALOAD_SIZE.
// ---------------------------------------------------------------------------

#include "FileSys.h"

namespace Neuron
{
  // -----------------------------------------------------------------------
  // BinaryDataWriter — writes structured data into a growable byte_buffer_t
  // -----------------------------------------------------------------------
  class BinaryDataWriter
  {
  public:
    BinaryDataWriter() = default;
    explicit BinaryDataWriter(size_t _reserveBytes) { m_buffer.reserve(_reserveBytes); }

    template <typename T>
    void Write(const T& _value)
    {
      static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
      const auto* bytes = reinterpret_cast<const uint8_t*>(&_value);
      m_buffer.insert(m_buffer.end(), bytes, bytes + sizeof(T));
    }

    void WriteString(const std::string& _value)
    {
      Write<uint32_t>(static_cast<uint32_t>(_value.size()));
      const auto* bytes = reinterpret_cast<const uint8_t*>(_value.data());
      m_buffer.insert(m_buffer.end(), bytes, bytes + _value.size());
    }

    void WriteBytes(const uint8_t* _data, size_t _count)
    {
      m_buffer.insert(m_buffer.end(), _data, _data + _count);
    }

    [[nodiscard]] const byte_buffer_t& Buffer() const noexcept { return m_buffer; }
    [[nodiscard]] byte_buffer_t&& ReleaseBuffer() noexcept { return std::move(m_buffer); }
    [[nodiscard]] size_t Size() const noexcept { return m_buffer.size(); }

    void Clear() { m_buffer.clear(); }

  private:
    byte_buffer_t m_buffer;
  };

  // -----------------------------------------------------------------------
  // BinaryDataReader — reads structured data from a byte_buffer_t with a cursor
  // -----------------------------------------------------------------------
  class BinaryDataReader
  {
  public:
    BinaryDataReader() = default;

    explicit BinaryDataReader(byte_buffer_t _data)
      : m_buffer(std::move(_data))
    {
    }

    BinaryDataReader(const uint8_t* _data, size_t _size)
      : m_buffer(_data, _data + _size)
    {
    }

    template <typename T>
    [[nodiscard]] T Read()
    {
      static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
      ASSERT_TEXT(m_cursor + sizeof(T) <= m_buffer.size(), "BinaryDataReader::Read out of bounds");

      T value;
      std::memcpy(&value, m_buffer.data() + m_cursor, sizeof(T));
      m_cursor += sizeof(T);
      return value;
    }

    [[nodiscard]] std::string ReadString()
    {
      uint32_t len = Read<uint32_t>();
      ASSERT_TEXT(m_cursor + len <= m_buffer.size(), "BinaryDataReader::ReadString out of bounds");

      std::string value(reinterpret_cast<const char*>(m_buffer.data() + m_cursor), len);
      m_cursor += len;
      return value;
    }

    void ReadBytes(uint8_t* _out, size_t _count)
    {
      ASSERT_TEXT(m_cursor + _count <= m_buffer.size(), "BinaryDataReader::ReadBytes out of bounds");
      std::memcpy(_out, m_buffer.data() + m_cursor, _count);
      m_cursor += _count;
    }

    [[nodiscard]] size_t Remaining() const noexcept { return m_buffer.size() - m_cursor; }
    [[nodiscard]] size_t Size() const noexcept { return m_buffer.size(); }
    [[nodiscard]] size_t Cursor() const noexcept { return m_cursor; }
    [[nodiscard]] bool IsEnd() const noexcept { return m_cursor >= m_buffer.size(); }

    void Reset() noexcept { m_cursor = 0; }

  private:
    byte_buffer_t m_buffer;
    size_t m_cursor = 0;
  };
}
