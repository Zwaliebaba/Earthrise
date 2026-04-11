#pragma once

#include "GameTypes/EntityHandle.h"

namespace GameLogic
{
  // EntityPool — fixed-capacity slot-map for entity allocation.
  // Provides O(1) alloc, free, and lookup by EntityHandle.
  // Server-only.
  class EntityPool
  {
  public:
    explicit EntityPool(uint32_t _capacity);

    // Allocate a new entity handle. Returns EntityHandle::Invalid() if pool is exhausted.
    [[nodiscard]] Neuron::EntityHandle Alloc() noexcept;

    // Free a handle, incrementing its generation for reuse detection.
    void Free(Neuron::EntityHandle _handle) noexcept;

    // Check if a handle is currently valid (allocated and generation matches).
    [[nodiscard]] bool IsValid(Neuron::EntityHandle _handle) const noexcept;

    // Total allocated (active) entities.
    [[nodiscard]] uint32_t ActiveCount() const noexcept { return m_activeCount; }

    // Pool capacity.
    [[nodiscard]] uint32_t Capacity() const noexcept { return m_capacity; }

  private:
    struct Slot
    {
      uint32_t Generation = 0;
      uint32_t NextFree   = 0; // Index of next free slot (forms a free-list)
      bool     Active     = false;
    };

    std::vector<Slot> m_slots;
    uint32_t          m_capacity    = 0;
    uint32_t          m_freeHead    = 0; // Index of first free slot
    uint32_t          m_activeCount = 0;

    static constexpr uint32_t INVALID_SLOT = UINT32_MAX;
  };
}
