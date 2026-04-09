#pragma once

#include "NetLib.h"

namespace Neuron
{
  class DataWriter
  {
  public:
    DataWriter() = default;

    // Write a single byte
    void WriteChar(const char _value)
    {
      ASSERT_TEXT(m_size + sizeof(_value) <= DATALOAD_SIZE, "WriteChar out of bounds");
      m_data[m_size++] = static_cast<std::byte>(_value);
    }

    void WriteByte(const uint8_t _value)
    {
      ASSERT_TEXT(m_size + sizeof(_value) <= DATALOAD_SIZE, "WriteByte out of bounds");
      m_data[m_size++] = static_cast<std::byte>(_value);
    }

    // Write an int16
    void WriteInt16(const int16_t _value)
    {
      ASSERT_TEXT(m_size + sizeof(_value) <= DATALOAD_SIZE, "WriteInt16 out of bounds");
      std::copy_n(reinterpret_cast<const uint8_t*>(&_value), sizeof(_value), reinterpret_cast<uint8_t*>(m_data.data() + m_size));
      m_size += sizeof(_value);
    }

    void WriteUInt16(const uint16_t _value)
    {
      ASSERT_TEXT(m_size + sizeof(_value) <= DATALOAD_SIZE, "WriteUInt16 out of bounds");
      std::copy_n(reinterpret_cast<const uint8_t*>(&_value), sizeof(_value), reinterpret_cast<uint8_t*>(m_data.data() + m_size));
      m_size += sizeof(_value);
    }

    // Write an int32
    void WriteInt32(const int32_t _value)
    {
      ASSERT_TEXT(m_size + sizeof(_value) <= DATALOAD_SIZE, "WriteInt32 out of bounds");
      std::copy_n(reinterpret_cast<const uint8_t*>(&_value), sizeof(_value), reinterpret_cast<uint8_t*>(m_data.data() + m_size));
      m_size += sizeof(_value);
    }

    void WriteUInt32(const uint32_t _value)
    {
      ASSERT_TEXT(m_size + sizeof(_value) <= DATALOAD_SIZE, "WriteUInt32 out of bounds");
      std::copy_n(reinterpret_cast<const uint8_t*>(&_value), sizeof(_value), reinterpret_cast<uint8_t*>(m_data.data() + m_size));
      m_size += sizeof(_value);
    }

    // Write an int64
    void WriteInt64(const int64_t _value)
    {
      ASSERT_TEXT(m_size + sizeof(_value) <= DATALOAD_SIZE, "WriteInt64 out of bounds");
      std::copy_n(reinterpret_cast<const uint8_t*>(&_value), sizeof(_value), reinterpret_cast<uint8_t*>(m_data.data() + m_size));
      m_size += sizeof(_value);
    }

    void WriteUInt64(const uint64_t _value)
    {
      ASSERT_TEXT(m_size + sizeof(_value) <= DATALOAD_SIZE, "WriteUInt64 out of bounds");
      std::copy_n(reinterpret_cast<const uint8_t*>(&_value), sizeof(_value), reinterpret_cast<uint8_t*>(m_data.data() + m_size));
      m_size += sizeof(_value);
    }

    // Write a float
    void WriteFloat(const float _value)
    {
      ASSERT_TEXT(m_size + sizeof(_value) <= DATALOAD_SIZE, "WriteFloat out of bounds");
      std::copy_n(reinterpret_cast<const uint8_t*>(&_value), sizeof(_value), reinterpret_cast<uint8_t*>(m_data.data() + m_size));
      m_size += sizeof(_value);
    }

    // Write a string (length-prefixed)
    void WriteString(const std::string& _value)
    {
      ASSERT_TEXT(m_size + sizeof(uint32_t) + _value.size() <= DATALOAD_SIZE, "WriteString out of bounds");
      uint32_t len = static_cast<uint32_t>(_value.size());

      WriteUInt32(len);
      std::memcpy(m_data.data() + m_size, _value.data(), len);
      m_size += len;
    }

    // Clear the buffer
    void Clear()
    {
      m_size = 0;
    }

    // Get the buffer
    [[nodiscard]] const char* Data() const { return reinterpret_cast<const char*>(m_data.data()); }

    // Get the size of the buffer
    [[nodiscard]] int Size() const { return static_cast<int>(m_size); }

  protected:
    std::array<std::byte, DATALOAD_SIZE> m_data;
    size_t m_size = 0;
  };
}
