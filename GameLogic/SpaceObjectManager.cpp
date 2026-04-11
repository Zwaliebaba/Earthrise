#include "pch.h"
#include "SpaceObjectManager.h"

namespace GameLogic
{
  SpaceObjectManager::SpaceObjectManager()
    : m_pool(MAX_ENTITIES)
  {
    m_spaceObjects.resize(MAX_ENTITIES);
    m_asteroidData.resize(MAX_ENTITIES);
    m_crateData.resize(MAX_ENTITIES);
    m_decorationData.resize(MAX_ENTITIES);
    m_shipData.resize(MAX_ENTITIES);
    m_jumpgateData.resize(MAX_ENTITIES);
    m_projectileData.resize(MAX_ENTITIES);
    m_stationData.resize(MAX_ENTITIES);
    m_turretData.resize(MAX_ENTITIES);
  }

  Neuron::EntityHandle SpaceObjectManager::CreateEntity(
    Neuron::SpaceObjectCategory _category,
    const XMFLOAT3& _position,
    uint32_t _meshNameHash,
    uint16_t _defIndex)
  {
    Neuron::EntityHandle handle = m_pool.Alloc();
    if (!handle.IsValid())
      return handle;

    uint32_t idx = HandleToIndex(handle);

    // Initialize common component.
    Neuron::SpaceObject& obj = m_spaceObjects[idx];
    obj = Neuron::SpaceObject{};
    obj.Handle        = handle;
    obj.Category      = _category;
    obj.Active        = true;
    obj.Position      = _position;
    obj.MeshNameHash  = _meshNameHash;
    obj.DefIndex      = _defIndex;

    // Initialize per-category component.
    switch (_category)
    {
    case Neuron::SpaceObjectCategory::Asteroid:    m_asteroidData[idx] = Neuron::AsteroidData{ handle };    break;
    case Neuron::SpaceObjectCategory::Crate:       m_crateData[idx] = Neuron::CrateData{ handle };          break;
    case Neuron::SpaceObjectCategory::Decoration:  m_decorationData[idx] = Neuron::DecorationData{ handle }; break;
    case Neuron::SpaceObjectCategory::Ship:        m_shipData[idx] = Neuron::ShipData{ handle };            break;
    case Neuron::SpaceObjectCategory::Jumpgate:    m_jumpgateData[idx] = Neuron::JumpgateData{ handle };    break;
    case Neuron::SpaceObjectCategory::Projectile:  m_projectileData[idx] = Neuron::ProjectileData{ handle }; break;
    case Neuron::SpaceObjectCategory::Station:     m_stationData[idx] = Neuron::StationData{ handle };      break;
    case Neuron::SpaceObjectCategory::Turret:      m_turretData[idx] = Neuron::TurretData{ handle };        break;
    default: break;
    }

    return handle;
  }

  void SpaceObjectManager::DestroyEntity(Neuron::EntityHandle _handle)
  {
    if (!m_pool.IsValid(_handle))
      return;

    uint32_t idx = HandleToIndex(_handle);

    m_spaceObjects[idx].Active = false;
    m_spaceObjects[idx] = Neuron::SpaceObject{};

    m_pool.Free(_handle);
  }

  bool SpaceObjectManager::IsValid(Neuron::EntityHandle _handle) const noexcept
  {
    return m_pool.IsValid(_handle);
  }

  Neuron::SpaceObject* SpaceObjectManager::GetSpaceObject(Neuron::EntityHandle _handle) noexcept
  {
    if (!m_pool.IsValid(_handle))
      return nullptr;
    return &m_spaceObjects[HandleToIndex(_handle)];
  }

  const Neuron::SpaceObject* SpaceObjectManager::GetSpaceObject(Neuron::EntityHandle _handle) const noexcept
  {
    if (!m_pool.IsValid(_handle))
      return nullptr;
    return &m_spaceObjects[HandleToIndex(_handle)];
  }

  // Helper macro to reduce boilerplate for per-category accessors.
  #define CATEGORY_ACCESSOR(TypeName, MemberArray, CategoryEnum)                    \
    Neuron::TypeName* SpaceObjectManager::Get##TypeName(Neuron::EntityHandle _handle) noexcept \
    {                                                                                \
      if (!m_pool.IsValid(_handle))                                                  \
        return nullptr;                                                              \
      uint32_t idx = HandleToIndex(_handle);                                         \
      if (m_spaceObjects[idx].Category != Neuron::SpaceObjectCategory::CategoryEnum) \
        return nullptr;                                                              \
      return &MemberArray[idx];                                                      \
    }

  CATEGORY_ACCESSOR(AsteroidData,    m_asteroidData,    Asteroid)
  CATEGORY_ACCESSOR(CrateData,       m_crateData,       Crate)
  CATEGORY_ACCESSOR(DecorationData,  m_decorationData,  Decoration)
  CATEGORY_ACCESSOR(ShipData,        m_shipData,        Ship)
  CATEGORY_ACCESSOR(JumpgateData,    m_jumpgateData,    Jumpgate)
  CATEGORY_ACCESSOR(ProjectileData,  m_projectileData,  Projectile)
  CATEGORY_ACCESSOR(StationData,     m_stationData,     Station)
  CATEGORY_ACCESSOR(TurretData,      m_turretData,      Turret)

  #undef CATEGORY_ACCESSOR
}
