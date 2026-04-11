#pragma once

#include "GameTypes/EntityHandle.h"

namespace GameLogic
{
  // SpatialHash — 3D hash-mapped spatial partitioning for broad-phase queries.
  // Only occupied cells consume memory (hash-mapped, not grid-allocated).
  // Cell size: 2 km (2000 units) per axis.
  class SpatialHash
  {
  public:
    static constexpr float CELL_SIZE = 2000.0f;

    // Insert an entity at the given world position.
    void Insert(Neuron::EntityHandle _handle, FXMVECTOR _position);

    // Remove an entity from its current cell.
    void Remove(Neuron::EntityHandle _handle);

    // Update an entity's position (remove + insert).
    void Update(Neuron::EntityHandle _handle, FXMVECTOR _oldPosition, FXMVECTOR _newPosition);

    // Query all entities in cells overlapping a sphere defined by _center and _radius.
    // Callback signature: void(Neuron::EntityHandle).
    template <typename Func>
    void QuerySphere(FXMVECTOR _center, float _radius, Func&& _func) const
    {
      float cx = XMVectorGetX(_center);
      float cy = XMVectorGetY(_center);
      float cz = XMVectorGetZ(_center);

      int minX = CellCoord(cx - _radius);
      int maxX = CellCoord(cx + _radius);
      int minY = CellCoord(cy - _radius);
      int maxY = CellCoord(cy + _radius);
      int minZ = CellCoord(cz - _radius);
      int maxZ = CellCoord(cz + _radius);

      for (int x = minX; x <= maxX; ++x)
        for (int y = minY; y <= maxY; ++y)
          for (int z = minZ; z <= maxZ; ++z)
          {
            auto it = m_cells.find(PackKey(x, y, z));
            if (it != m_cells.end())
            {
              for (auto handle : it->second)
                _func(handle);
            }
          }
    }

    // Clear all cells.
    void Clear() noexcept;

    // Number of occupied cells (for diagnostics).
    [[nodiscard]] size_t OccupiedCellCount() const noexcept { return m_cells.size(); }

    // Total entity count across all cells.
    [[nodiscard]] size_t TotalEntityCount() const noexcept { return m_entityCells.size(); }

  private:
    // Convert a world coordinate to a cell coordinate.
    [[nodiscard]] static int CellCoord(float _worldCoord) noexcept
    {
      return static_cast<int>(std::floor(_worldCoord / CELL_SIZE));
    }

    // Pack 3 cell coordinates into a single 64-bit key.
    [[nodiscard]] static uint64_t PackKey(int _x, int _y, int _z) noexcept
    {
      // Use 21-bit signed integers packed into 64 bits.
      // Range: [-1048576, 1048575] which covers ±2 billion units at 2000 cell size.
      auto pack21 = [](int v) -> uint64_t { return static_cast<uint64_t>(v) & 0x1FFFFFu; };
      return (pack21(_x) << 42) | (pack21(_y) << 21) | pack21(_z);
    }

    [[nodiscard]] static uint64_t PackKeyFromPosition(FXMVECTOR _pos) noexcept
    {
      return PackKey(
        CellCoord(XMVectorGetX(_pos)),
        CellCoord(XMVectorGetY(_pos)),
        CellCoord(XMVectorGetZ(_pos)));
    }

    // Cell key → list of entity handles in that cell.
    std::unordered_map<uint64_t, std::vector<Neuron::EntityHandle>> m_cells;

    // Entity handle → cell key (for fast removal).
    std::unordered_map<uint32_t, uint64_t> m_entityCells;
  };
}
