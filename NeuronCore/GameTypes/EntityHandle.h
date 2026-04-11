#pragma once

// EntityHandle — typed (index, generation) handle for entity slot-map lookup.
// 32-bit compact representation: 20-bit index, 12-bit generation.
// Populated in Phase 2.

namespace Neuron
{
  struct EntityHandle
  {
    static constexpr uint32_t INDEX_BITS      = 20;
    static constexpr uint32_t GENERATION_BITS = 12;
    static constexpr uint32_t MAX_INDEX       = (1u << INDEX_BITS) - 1;        // 1,048,575
    static constexpr uint32_t MAX_GENERATION  = (1u << GENERATION_BITS) - 1;   // 4,095

    uint32_t m_id = 0;

    constexpr EntityHandle() noexcept = default;
    constexpr explicit EntityHandle(uint32_t _index, uint32_t _generation) noexcept
      : m_id((_generation << INDEX_BITS) | (_index & MAX_INDEX))
    {
    }

    [[nodiscard]] constexpr uint32_t Index() const noexcept { return m_id & MAX_INDEX; }
    [[nodiscard]] constexpr uint32_t Generation() const noexcept { return m_id >> INDEX_BITS; }
    [[nodiscard]] constexpr bool IsValid() const noexcept { return m_id != 0; }

    constexpr bool operator==(const EntityHandle&) const noexcept = default;
    constexpr auto operator<=>(const EntityHandle&) const noexcept = default;

    static constexpr EntityHandle Invalid() noexcept { return EntityHandle{}; }
  };
}
