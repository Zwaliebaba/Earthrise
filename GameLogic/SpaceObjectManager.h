#pragma once

#include "EntityPool.h"
#include "GameTypes/SpaceObject.h"
#include "GameTypes/AsteroidData.h"
#include "GameTypes/CrateData.h"
#include "GameTypes/DecorationData.h"
#include "GameTypes/ShipData.h"
#include "GameTypes/JumpgateData.h"
#include "GameTypes/ProjectileData.h"
#include "GameTypes/StationData.h"
#include "GameTypes/TurretData.h"

namespace GameLogic
{
  static constexpr uint32_t MAX_ENTITIES = 16384;

  // SpaceObjectManager — owns the entity pool and all component arrays.
  // Creates/destroys entities by category. Provides O(1) lookup by EntityHandle.
  // Server-only.
  class SpaceObjectManager
  {
  public:
    SpaceObjectManager();

    // Create an entity of the given category. Returns the handle, or Invalid() on failure.
    [[nodiscard]] Neuron::EntityHandle CreateEntity(
      Neuron::SpaceObjectCategory _category,
      const XMFLOAT3& _position = {},
      uint32_t _meshNameHash = 0,
      uint16_t _defIndex = 0);

    // Destroy an entity and free its slot.
    void DestroyEntity(Neuron::EntityHandle _handle);

    // Check if a handle is currently valid.
    [[nodiscard]] bool IsValid(Neuron::EntityHandle _handle) const noexcept;

    // Common component access.
    [[nodiscard]] Neuron::SpaceObject*       GetSpaceObject(Neuron::EntityHandle _handle) noexcept;
    [[nodiscard]] const Neuron::SpaceObject* GetSpaceObject(Neuron::EntityHandle _handle) const noexcept;

    // Per-category component access. Returns nullptr if handle is invalid or wrong category.
    [[nodiscard]] Neuron::AsteroidData*    GetAsteroidData(Neuron::EntityHandle _handle) noexcept;
    [[nodiscard]] Neuron::CrateData*       GetCrateData(Neuron::EntityHandle _handle) noexcept;
    [[nodiscard]] Neuron::DecorationData*  GetDecorationData(Neuron::EntityHandle _handle) noexcept;
    [[nodiscard]] Neuron::ShipData*        GetShipData(Neuron::EntityHandle _handle) noexcept;
    [[nodiscard]] Neuron::JumpgateData*    GetJumpgateData(Neuron::EntityHandle _handle) noexcept;
    [[nodiscard]] Neuron::ProjectileData*  GetProjectileData(Neuron::EntityHandle _handle) noexcept;
    [[nodiscard]] Neuron::StationData*     GetStationData(Neuron::EntityHandle _handle) noexcept;
    [[nodiscard]] Neuron::TurretData*      GetTurretData(Neuron::EntityHandle _handle) noexcept;

    // Active entity count.
    [[nodiscard]] uint32_t ActiveCount() const noexcept { return m_pool.ActiveCount(); }

    // Iterate all active SpaceObjects. Callback signature: void(Neuron::SpaceObject&).
    template <typename Func>
    void ForEachActive(Func&& _func)
    {
      for (auto& obj : m_spaceObjects)
      {
        if (obj.Active)
          _func(obj);
      }
    }

    // Iterate active entities of a specific category.
    template <typename Func>
    void ForEachCategory(Neuron::SpaceObjectCategory _category, Func&& _func)
    {
      for (auto& obj : m_spaceObjects)
      {
        if (obj.Active && obj.Category == _category)
          _func(obj);
      }
    }

  private:
    // Convert handle to 0-based array index.
    [[nodiscard]] uint32_t HandleToIndex(Neuron::EntityHandle _handle) const noexcept
    {
      return _handle.Index() - 1; // Pool uses 1-based indices
    }

    EntityPool m_pool;

    // Common component array — indexed by slot (handle.Index() - 1).
    std::vector<Neuron::SpaceObject> m_spaceObjects;

    // Per-category SoA component arrays — same indexing.
    std::vector<Neuron::AsteroidData>    m_asteroidData;
    std::vector<Neuron::CrateData>       m_crateData;
    std::vector<Neuron::DecorationData>  m_decorationData;
    std::vector<Neuron::ShipData>        m_shipData;
    std::vector<Neuron::JumpgateData>    m_jumpgateData;
    std::vector<Neuron::ProjectileData>  m_projectileData;
    std::vector<Neuron::StationData>     m_stationData;
    std::vector<Neuron::TurretData>      m_turretData;
  };
}
