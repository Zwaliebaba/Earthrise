#include "pch.h"
#include "EntityPool.h"

namespace GameLogic
{
  EntityPool::EntityPool(uint32_t _capacity)
    : m_capacity(_capacity)
  {
    ASSERT_TEXT(_capacity > 0 && _capacity <= Neuron::EntityHandle::MAX_INDEX,
      "EntityPool capacity out of range");

    m_slots.resize(_capacity);

    // Build the free list — each slot points to the next.
    for (uint32_t i = 0; i < _capacity; ++i)
    {
      m_slots[i].NextFree = (i + 1 < _capacity) ? i + 1 : INVALID_SLOT;
    }

    m_freeHead = 0;
  }

  Neuron::EntityHandle EntityPool::Alloc() noexcept
  {
    if (m_freeHead == INVALID_SLOT)
      return Neuron::EntityHandle::Invalid();

    uint32_t index = m_freeHead;
    Slot& slot = m_slots[index];

    m_freeHead = slot.NextFree;
    slot.Active = true;
    ++m_activeCount;

    // Use index + 1 so that index 0 with generation 0 produces a non-zero handle.
    // Actually, EntityHandle(0, 0) would be m_id=0 which is Invalid().
    // We use 1-based index to avoid that: slot[0] gets handle index 1.
    return Neuron::EntityHandle(index + 1, slot.Generation);
  }

  void EntityPool::Free(Neuron::EntityHandle _handle) noexcept
  {
    if (!_handle.IsValid())
      return;

    uint32_t index = _handle.Index() - 1; // Convert back to 0-based slot index
    if (index >= m_capacity)
      return;

    Slot& slot = m_slots[index];

    if (!slot.Active || slot.Generation != _handle.Generation())
      return;

    slot.Active = false;
    slot.Generation = (slot.Generation + 1) & Neuron::EntityHandle::MAX_GENERATION;
    slot.NextFree = m_freeHead;
    m_freeHead = index;
    --m_activeCount;
  }

  bool EntityPool::IsValid(Neuron::EntityHandle _handle) const noexcept
  {
    if (!_handle.IsValid())
      return false;

    uint32_t index = _handle.Index() - 1;
    if (index >= m_capacity)
      return false;

    const Slot& slot = m_slots[index];
    return slot.Active && slot.Generation == _handle.Generation();
  }
}
