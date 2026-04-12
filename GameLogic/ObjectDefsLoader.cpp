#include "pch.h"
#include "ObjectDefsLoader.h"

namespace GameLogic
{
  void ObjectDefsLoader::ReadBaseDef(Neuron::BinaryDataReader& _reader, Neuron::ObjectDefBase& _def)
  {
    _def.DefId         = _reader.Read<uint16_t>();
    _def.Category      = _reader.Read<Neuron::SpaceObjectCategory>();
    _def.Name          = _reader.ReadString();
    _def.MeshName      = _reader.ReadString();
    _def.MeshNameHash  = Neuron::HashMeshName(_def.MeshName);
    _def.BoundingRadius = _reader.Read<float>();
    _def.DefaultColor  = _reader.Read<XMFLOAT4>();
  }

  bool ObjectDefsLoader::LoadFromFile(const std::wstring& _filePath)
  {
    byte_buffer_t buffer = BinaryFile::ReadFile(_filePath);
    if (buffer.empty())
      return false;

    Neuron::BinaryDataReader reader(buffer);

    uint32_t totalCount = reader.Read<uint32_t>();

    for (uint32_t i = 0; i < totalCount; ++i)
    {
      // Peek at category to determine which def type to read.
      size_t savedCursor = reader.Position();

      [[maybe_unused]] uint16_t defId = reader.Read<uint16_t>();
      auto category = reader.Read<Neuron::SpaceObjectCategory>();

      // Seek back to read the full entry including base fields.
      reader.Seek(savedCursor);

      switch (category)
      {
      case SpaceObjectCategory::Asteroid:
      {
        AsteroidDef def;
        ReadBaseDef(reader, def);
        def.MinRotationSpeed = reader.Read<float>();
        def.MaxRotationSpeed = reader.Read<float>();
        def.ResourceType     = reader.Read<uint32_t>();
        def.MaxResource      = reader.Read<float>();
        def.Surface          = reader.Read<Neuron::SurfaceType>();
        m_asteroidDefs.push_back(std::move(def));
        break;
      }
      case SpaceObjectCategory::Crate:
      {
        CrateDef def;
        ReadBaseDef(reader, def);
        def.LootTableId = reader.Read<uint32_t>();
        def.Lifetime    = reader.Read<float>();
        m_crateDefs.push_back(std::move(def));
        break;
      }
      case SpaceObjectCategory::Decoration:
      {
        DecorationDef def;
        ReadBaseDef(reader, def);
        def.MinRotationSpeed = reader.Read<float>();
        def.MaxRotationSpeed = reader.Read<float>();
        m_decorationDefs.push_back(std::move(def));
        break;
      }
      case SpaceObjectCategory::Ship:
      {
        ShipDef def;
        ReadBaseDef(reader, def);
        def.ShieldMaxHP  = reader.Read<float>();
        def.ArmorMaxHP   = reader.Read<float>();
        def.HullMaxHP    = reader.Read<float>();
        def.EnergyMax    = reader.Read<float>();
        def.EnergyRegen  = reader.Read<float>();
        def.MaxSpeed     = reader.Read<float>();
        def.TurnRate     = reader.Read<float>();
        def.Thrust       = reader.Read<float>();
        def.TurretSlots  = reader.Read<uint8_t>();
        m_shipDefs.push_back(std::move(def));
        break;
      }
      case SpaceObjectCategory::Jumpgate:
      {
        JumpgateDef def;
        ReadBaseDef(reader, def);
        def.ActivationRadius = reader.Read<float>();
        m_jumpgateDefs.push_back(std::move(def));
        break;
      }
      case SpaceObjectCategory::Projectile:
      {
        ProjectileDef def;
        ReadBaseDef(reader, def);
        def.Damage   = reader.Read<float>();
        def.Speed    = reader.Read<float>();
        def.Lifetime = reader.Read<float>();
        def.TurnRate = reader.Read<float>();
        m_projectileDefs.push_back(std::move(def));
        break;
      }
      case SpaceObjectCategory::Station:
      {
        StationDef def;
        ReadBaseDef(reader, def);
        def.DockingRadius = reader.Read<float>();
        def.HasMarket     = reader.Read<bool>();
        def.HasRepair     = reader.Read<bool>();
        def.HasRefuel     = reader.Read<bool>();
        def.HasMissions   = reader.Read<bool>();
        m_stationDefs.push_back(std::move(def));
        break;
      }
      case SpaceObjectCategory::Turret:
      {
        TurretDef def;
        ReadBaseDef(reader, def);
        def.FireRate        = reader.Read<float>();
        def.Damage          = reader.Read<float>();
        def.Range           = reader.Read<float>();
        def.ProjectileSpeed = reader.Read<float>();
        def.TrackingSpeed   = reader.Read<float>();
        m_turretDefs.push_back(std::move(def));
        break;
      }
      default:
      {
        // Skip unknown categories by reading base def and discarding
        ObjectDefBase def;
        ReadBaseDef(reader, def);
        Neuron::DebugTrace("ObjectDefsLoader: Unknown category {} for def '{}'\n",
          static_cast<int>(category), def.Name);
        break;
      }
      }
    }

    Neuron::DebugTrace("ObjectDefsLoader: Loaded {} definitions from file\n", totalCount);
    return true;
  }

  const Neuron::ObjectDefBase* ObjectDefsLoader::FindDef(uint16_t _defId) const noexcept
  {
    auto searchIn = [_defId](const auto& _vec) -> const Neuron::ObjectDefBase*
    {
      for (const auto& def : _vec)
      {
        if (def.DefId == _defId)
          return &def;
      }
      return nullptr;
    };

    if (auto* p = searchIn(m_asteroidDefs))    return p;
    if (auto* p = searchIn(m_crateDefs))       return p;
    if (auto* p = searchIn(m_decorationDefs))  return p;
    if (auto* p = searchIn(m_shipDefs))        return p;
    if (auto* p = searchIn(m_jumpgateDefs))    return p;
    if (auto* p = searchIn(m_projectileDefs))  return p;
    if (auto* p = searchIn(m_stationDefs))     return p;
    if (auto* p = searchIn(m_turretDefs))      return p;

    return nullptr;
  }
}
