#pragma once

#include "Zone.h"
#include "SerializationBase.h"
#include "GameTypes/AsteroidCluster.h"

namespace EarthRise
{
  // ZoneLoader — loads zone definition from a binary data file.
  // Creates initial entities (asteroids, stations, jumpgates, decorations)
  // from the zone's definition data.
  class ZoneLoader
  {
  public:
    // Load a zone from a binary definition file and populate it with entities.
    // File format: [entityCount:uint32_t] [entries...]
    // Each entry: [category:uint8_t, defIndex:uint16_t, meshNameHash:uint32_t,
    //              position:XMFLOAT3, orientation:XMFLOAT4, color:XMFLOAT4,
    //              boundingRadius:float]
    static bool LoadFromFile(Zone& _zone, const std::wstring& _filePath);

    // Create a default test zone with hardcoded entities for development/testing.
    static void CreateTestZone(Zone& _zone);

    // Generate universe from seed and populate the zone with celestial bodies and asteroids.
    // Optionally applies delta state from a persistence file.
    static bool LoadUniverse(Zone& _zone, uint64_t _seed,
      const std::wstring& _deltaFilePath = L"");
  };
}
