#include "pch.h"
#include "UniversePersistence.h"

namespace GameLogic
{
  bool UniversePersistence::SaveDelta(const std::wstring& _filePath, uint64_t _seed,
    uint32_t _serverTick, const std::vector<Neuron::AsteroidCluster>& _clusters)
  {
    Neuron::BinaryDataWriter writer(4096);

    // Header.
    writer.Write<uint32_t>(MAGIC);
    writer.Write<uint32_t>(VERSION);
    writer.Write<uint64_t>(_seed);
    writer.Write<uint32_t>(_serverTick);
    writer.Write<uint32_t>(static_cast<uint32_t>(_clusters.size()));

    // Per cluster.
    for (const auto& cluster : _clusters)
    {
      writer.Write<uint16_t>(cluster.ClusterId);
      writer.Write<uint32_t>(cluster.ActiveCount);
      writer.Write<uint32_t>(static_cast<uint32_t>(cluster.RegrowthQueue.size()));

      for (const auto& entry : cluster.RegrowthQueue)
        writer.Write<float>(entry.Timer);

      // Depleted asteroid count is derived: AsteroidCount - ActiveCount - RegrowthQueue.size()
      // We don't track individual spawn indices in the current regrowth model,
      // so we store 0 depleted indices. Fully depleted asteroids that haven't
      // entered the regrowth queue yet are lost on restart (acceptable for v1).
      writer.Write<uint32_t>(0);
    }

    return BinaryFile::WriteFile(_filePath, writer.Buffer());
  }

  bool UniversePersistence::LoadDelta(const std::wstring& _filePath, uint64_t _expectedSeed,
    DeltaState& _outState)
  {
    byte_buffer_t buffer = BinaryFile::ReadFile(_filePath);
    if (buffer.empty())
      return false;

    Neuron::BinaryDataReader reader(buffer);

    // Validate header.
    uint32_t magic = reader.Read<uint32_t>();
    if (magic != MAGIC)
    {
      Neuron::DebugTrace("UniversePersistence: Invalid magic in delta file\n");
      return false;
    }

    uint32_t version = reader.Read<uint32_t>();
    if (version != VERSION)
    {
      Neuron::DebugTrace("UniversePersistence: Unsupported version {} in delta file\n", version);
      return false;
    }

    _outState.Seed = reader.Read<uint64_t>();
    if (_outState.Seed != _expectedSeed)
    {
      Neuron::DebugTrace("UniversePersistence: Seed mismatch (expected {}, got {})\n",
        _expectedSeed, _outState.Seed);
      return false;
    }

    _outState.ServerTick = reader.Read<uint32_t>();
    uint32_t clusterCount = reader.Read<uint32_t>();

    _outState.ClusterDeltas.resize(clusterCount);

    for (uint32_t i = 0; i < clusterCount; ++i)
    {
      auto& cd = _outState.ClusterDeltas[i];
      cd.ClusterId   = reader.Read<uint16_t>();
      cd.ActiveCount = reader.Read<uint32_t>();

      uint32_t regrowthCount = reader.Read<uint32_t>();
      cd.RegrowthTimers.resize(regrowthCount);
      for (uint32_t r = 0; r < regrowthCount; ++r)
        cd.RegrowthTimers[r] = reader.Read<float>();

      uint32_t depletedCount = reader.Read<uint32_t>();
      cd.DepletedSpawnIndices.resize(depletedCount);
      for (uint32_t d = 0; d < depletedCount; ++d)
        cd.DepletedSpawnIndices[d] = reader.Read<uint32_t>();
    }

    return true;
  }
}
