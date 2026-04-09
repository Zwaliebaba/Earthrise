#pragma once

#include "NetLib.h"

namespace Neuron
{
  template <class T>
  static constexpr bool IsReadableType = std::is_trivially_copyable_v<T>;

  class DataReader
  {
  public:
    DataReader() = default;

    DataReader(const uint8_t *_data, size_t _size)
    {
      ASSERT_TEXT(_size <= DATALOAD_SIZE, "DataReader initialized with too large data size");
      std::copy_n(_data, _size, reinterpret_cast<uint8_t *>(m_data.data()));
    }

    template <typename T>
    [[nodiscard]] T Read()
    {
      static_assert(IsReadableType<T>, "Type is not readable");

      T value;

      ASSERT_TEXT(m_size + sizeof(T) <= DATALOAD_SIZE, "Read out of bounds");

      std::copy_n(reinterpret_cast<const uint8_t *>(m_data.data() + m_size), sizeof(T), reinterpret_cast<uint8_t *>(&value));
      m_size += sizeof(T);
      return value;
    }

    template <typename T>
    void ReadArray(T *_array, size_t _count)
    {
      if constexpr (std::is_trivially_copyable_v<T>)
      {
        const size_t bytes = sizeof(T) * _count;
        ASSERT_TEXT(m_size + bytes <= DATALOAD_SIZE, "ReadArray out of bounds");
        std::memcpy(_array, m_data.data() + m_size, bytes);
        m_size += bytes;
      }
      else
      {
        for (size_t i = 0; i < _count; ++i)
        {
          _array[i] = Read<T>();
        }
      }
    }

    std::string ReadString()
    {
      uint32_t len = Read<uint32_t>();

      ASSERT_TEXT(m_size + len <= DATALOAD_SIZE, "ReadString out of bounds");
      std::string value(reinterpret_cast<const char *>(m_data.data() + m_size), len);
      m_size += len;
      return value;
    }

  protected:
    std::array<std::byte, DATALOAD_SIZE> m_data;
    size_t m_size = 0;
  };
}
