#pragma once

#include "GameTypes/AsteroidCluster.h"

namespace GameLogic
{
  // UniversePersistence — save/load delta state for the procedural universe.
  // The delta captures asteroid depletion state and regrowth timers so the
  // universe survives server restarts.
  class UniversePersistence
  {
  public:
    static constexpr uint32_t MAGIC   = 0x56494E55; // 'UNIV' (little-endian)
    static constexpr uint32_t VERSION = 1;

    struct ClusterDelta
    {
      uint16_t              ClusterId     = 0;
      uint32_t              ActiveCount   = 0;
      std::vector<float>    RegrowthTimers;
      std::vector<uint32_t> DepletedSpawnIndices;
    };

    struct DeltaState
    {
      uint64_t                  Seed          = 0;
      uint32_t                  ServerTick    = 0;
      std::vector<ClusterDelta> ClusterDeltas;
    };

    // Save delta state to a binary file.
    static bool SaveDelta(const std::wstring& _filePath, uint64_t _seed,
      uint32_t _serverTick, const std::vector<Neuron::AsteroidCluster>& _clusters);

    // Load delta state from a binary file. Returns false if file is missing or corrupt.
    static bool LoadDelta(const std::wstring& _filePath, uint64_t _expectedSeed,
      DeltaState& _outState);
  };
}
