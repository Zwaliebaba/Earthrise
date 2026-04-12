#pragma once

#include "GameTypes/ObjectDefs.h"

namespace GameLogic
{
  // ObjectDefsLoader — loads object definition data from binary files on disk.
  // Server-only. Client receives definitions via network messages or uses mesh names directly.
  class ObjectDefsLoader
  {
  public:
    // Load all definitions from a binary file.
    // File format: [count:uint32_t] [DefId:uint16_t, Category:uint8_t, NameLen:uint32_t, Name:chars,
    //               MeshNameLen:uint32_t, MeshName:chars, BoundingRadius:float, Color:float4,
    //               ...category-specific fields...]
    bool LoadFromFile(const std::wstring& _filePath);

    // Access loaded definitions by category.
    [[nodiscard]] const std::vector<Neuron::AsteroidDef>&    GetAsteroidDefs()    const noexcept { return m_asteroidDefs; }
    [[nodiscard]] const std::vector<Neuron::CrateDef>&       GetCrateDefs()       const noexcept { return m_crateDefs; }
    [[nodiscard]] const std::vector<Neuron::DecorationDef>&  GetDecorationDefs()  const noexcept { return m_decorationDefs; }
    [[nodiscard]] const std::vector<Neuron::ShipDef>&        GetShipDefs()        const noexcept { return m_shipDefs; }
    [[nodiscard]] const std::vector<Neuron::JumpgateDef>&    GetJumpgateDefs()    const noexcept { return m_jumpgateDefs; }
    [[nodiscard]] const std::vector<Neuron::ProjectileDef>&  GetProjectileDefs()  const noexcept { return m_projectileDefs; }
    [[nodiscard]] const std::vector<Neuron::StationDef>&     GetStationDefs()     const noexcept { return m_stationDefs; }
    [[nodiscard]] const std::vector<Neuron::TurretDef>&      GetTurretDefs()      const noexcept { return m_turretDefs; }
    [[nodiscard]] const std::vector<Neuron::PlanetDef>&      GetPlanetDefs()      const noexcept { return m_planetDefs; }
    [[nodiscard]] const std::vector<Neuron::SunDef>&         GetSunDefs()         const noexcept { return m_sunDefs; }

    // Lookup a definition by DefId (linear search — called infrequently at spawn time).
    [[nodiscard]] const Neuron::ObjectDefBase* FindDef(uint16_t _defId) const noexcept;

  private:
    void ReadBaseDef(Neuron::BinaryDataReader& _reader, Neuron::ObjectDefBase& _def);

    std::vector<Neuron::AsteroidDef>    m_asteroidDefs;
    std::vector<Neuron::CrateDef>       m_crateDefs;
    std::vector<Neuron::DecorationDef>  m_decorationDefs;
    std::vector<Neuron::ShipDef>        m_shipDefs;
    std::vector<Neuron::JumpgateDef>    m_jumpgateDefs;
    std::vector<Neuron::ProjectileDef>  m_projectileDefs;
    std::vector<Neuron::StationDef>     m_stationDefs;
    std::vector<Neuron::TurretDef>      m_turretDefs;
    std::vector<Neuron::PlanetDef>      m_planetDefs;
    std::vector<Neuron::SunDef>         m_sunDefs;
  };
}
