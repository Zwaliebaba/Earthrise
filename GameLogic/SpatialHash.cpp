#include "pch.h"
#include "SpatialHash.h"

namespace GameLogic
{
  void SpatialHash::Insert(Neuron::EntityHandle _handle, FXMVECTOR _position)
  {
    uint64_t key = PackKeyFromPosition(_position);
    m_cells[key].push_back(_handle);
    m_entityCells[_handle.m_id] = key;
  }

  void SpatialHash::Remove(Neuron::EntityHandle _handle)
  {
    auto entIt = m_entityCells.find(_handle.m_id);
    if (entIt == m_entityCells.end())
      return;

    uint64_t key = entIt->second;
    m_entityCells.erase(entIt);

    auto cellIt = m_cells.find(key);
    if (cellIt != m_cells.end())
    {
      auto& vec = cellIt->second;
      std::erase(vec, _handle);
      if (vec.empty())
        m_cells.erase(cellIt);
    }
  }

  void SpatialHash::Update(Neuron::EntityHandle _handle, FXMVECTOR _oldPosition, FXMVECTOR _newPosition)
  {
    uint64_t oldKey = PackKeyFromPosition(_oldPosition);
    uint64_t newKey = PackKeyFromPosition(_newPosition);

    if (oldKey == newKey)
      return; // Same cell, nothing to do.

    Remove(_handle);
    Insert(_handle, _newPosition);
  }

  void SpatialHash::Clear() noexcept
  {
    m_cells.clear();
    m_entityCells.clear();
  }
}
