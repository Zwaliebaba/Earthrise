#pragma once

namespace Neuron
{
  // CRTP base class providing Read<T>/Write<T> over a byte_buffer_t with cursor tracking.
  // Used by both BinaryDataReader (disk, read-only) and BinaryDataWriter (disk, write-only).
  // Not constrained by DATALOAD_SIZE — suitable for zone definitions, object defs, tuning data.
  template <typename Derived>
  class SerializationBase
  {
  public:
    [[nodiscard]] size_t Position() const noexcept { return m_cursor; }
    [[nodiscard]] size_t Size() const noexcept { return Self().BufferSize(); }
    [[nodiscard]] bool AtEnd() const noexcept { return m_cursor >= Size(); }

  protected:
    size_t m_cursor = 0;

  private:
    [[nodiscard]] const Derived& Self() const noexcept { return static_cast<const Derived&>(*this); }
    [[nodiscard]] Derived& Self() noexcept { return static_cast<Derived&>(*this); }
  };

  // BinaryDataReader — cursor-based reader over an existing byte_buffer_t.
  // Does not own the buffer; the caller must keep it alive.
  class BinaryDataReader : public SerializationBase<BinaryDataReader>
  {
  public:
    explicit BinaryDataReader(const byte_buffer_t& _buffer) noexcept
      : m_buffer(_buffer)
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

    template <typename T>
    void ReadArray(T* _array, size_t _count)
    {
      static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
      const size_t bytes = sizeof(T) * _count;
      ASSERT_TEXT(m_cursor + bytes <= m_buffer.size(), "BinaryDataReader::ReadArray out of bounds");

      std::memcpy(_array, m_buffer.data() + m_cursor, bytes);
      m_cursor += bytes;
    }

    [[nodiscard]] std::string ReadString()
    {
      uint32_t len = Read<uint32_t>();
      ASSERT_TEXT(m_cursor + len <= m_buffer.size(), "BinaryDataReader::ReadString out of bounds");

      std::string value(reinterpret_cast<const char*>(m_buffer.data() + m_cursor), len);
      m_cursor += len;
      return value;
    }

    void Seek(size_t _position) noexcept { m_cursor = _position; }

    [[nodiscard]] size_t BufferSize() const noexcept { return m_buffer.size(); }

  private:
    const byte_buffer_t& m_buffer;
  };

  // BinaryDataWriter — cursor-based writer that appends to a byte_buffer_t.
  // Owns and grows the buffer as needed.
  class BinaryDataWriter : public SerializationBase<BinaryDataWriter>
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
      m_cursor += sizeof(T);
    }

    template <typename T>
    void WriteArray(const T* _array, size_t _count)
    {
      static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
      const size_t bytes = sizeof(T) * _count;

      const auto* raw = reinterpret_cast<const uint8_t*>(_array);
      m_buffer.insert(m_buffer.end(), raw, raw + bytes);
      m_cursor += bytes;
    }

    void WriteString(const std::string& _value)
    {
      Write<uint32_t>(static_cast<uint32_t>(_value.size()));
      m_buffer.insert(m_buffer.end(), _value.begin(), _value.end());
      m_cursor += _value.size();
    }

    [[nodiscard]] const byte_buffer_t& Buffer() const noexcept { return m_buffer; }
    [[nodiscard]] byte_buffer_t&& MoveBuffer() noexcept { return std::move(m_buffer); }
    [[nodiscard]] size_t BufferSize() const noexcept { return m_buffer.size(); }

    void Clear() { m_buffer.clear(); m_cursor = 0; }

  private:
    byte_buffer_t m_buffer;
  };
}
