#pragma once

#include "SpaceObjectManager.h"
#include "MiningSystem.h"
#include "GameTypes/AsteroidCluster.h"
#include "NeuronRandom.h"

namespace GameLogic
{
  // RegrowthSystem — tracks depleted asteroids per cluster and respawns them after a cooldown.
  class RegrowthSystem
  {
  public:
    RegrowthSystem(SpaceObjectManager& _manager, std::vector<Neuron::AsteroidCluster>& _clusters);

    // Seed the internal PRNG (call after universe generation).
    void Seed(uint64_t _universeSeed);

    // Enqueue all depletions from the mining system for this tick.
    void EnqueueDepletions(const MiningSystem& _mining);

    // Tick regrowth timers and respawn asteroids as needed.
    void Update(float _deltaTime);

  private:
    XMFLOAT3 GenerateSpawnPosition(const Neuron::AsteroidCluster& _cluster);

    SpaceObjectManager&                  m_manager;
    std::vector<Neuron::AsteroidCluster>& m_clusters;
    Neuron::NeuronRandom                 m_rng;
  };
}
