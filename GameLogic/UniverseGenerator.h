#pragma once

#include "SpaceObjectManager.h"
#include "GameTypes/AsteroidCluster.h"
#include "GameTypes/PlanetData.h"
#include "GameTypes/SunData.h"
#include "NeuronRandom.h"

namespace GameLogic
{
  // UniverseGenerator — deterministic, seed-based procedural generation.
  // Given the same seed, produces the same universe layout every time.
  class UniverseGenerator
  {
  public:
    struct UniverseLayout
    {
      struct CelestialBody
      {
        Neuron::SpaceObjectCategory Category = Neuron::SpaceObjectCategory::Sun;
        XMFLOAT3            Position       = {};
        float               BoundingRadius = 1.0f;
        XMFLOAT4            Color          = { 1.0f, 1.0f, 1.0f, 1.0f };
        uint32_t            MeshNameHash   = 0;
        Neuron::SurfaceType Surface        = Neuron::SurfaceType::Default;
        Neuron::PlanetData  PlanetInit     = {};
        Neuron::SunData     SunInit        = {};
      };

      struct AsteroidSpawn
      {
        XMFLOAT3  Position       = {};
        float     BoundingRadius = 1.0f;
        XMFLOAT4  Color          = { 1.0f, 1.0f, 1.0f, 1.0f };
        uint32_t  MeshNameHash   = 0;
        Neuron::SurfaceType Surface = Neuron::SurfaceType::Default;
        float     RotationSpeed  = 0.0f;
        XMFLOAT3  RotationAxis   = { 0.0f, 1.0f, 0.0f };
        uint32_t  ResourceType   = 0;
        float     ResourceAmount = 0.0f;
        uint16_t  ClusterId      = 0;
      };

      uint64_t                            Seed = 0;
      std::vector<CelestialBody>          Bodies;
      std::vector<Neuron::AsteroidCluster> Clusters;
      std::vector<AsteroidSpawn>          Asteroids;
    };

    // Generate a complete universe layout from a seed.
    [[nodiscard]] static UniverseLayout Generate(uint64_t _seed);

  private:
    // Internal generation stages.
    static void GenerateSuns(Neuron::NeuronRandom& _rng, UniverseLayout& _layout);
    static void GeneratePlanets(Neuron::NeuronRandom& _rng, UniverseLayout& _layout);
    static void GenerateRingClusters(Neuron::NeuronRandom& _rng, UniverseLayout& _layout);
    static void GenerateSupernovaClusters(Neuron::NeuronRandom& _rng, UniverseLayout& _layout);
    static void PopulateClusterAsteroids(Neuron::NeuronRandom& _rng, UniverseLayout& _layout);

    // Shape sampling helpers.
    static XMFLOAT3 RandomPointInTorus(Neuron::NeuronRandom& _rng,
      FXMVECTOR _center, FXMVECTOR _orientation,
      float _innerR, float _outerR, float _thickness);

    static XMFLOAT3 RandomPointInSupernovaCloud(Neuron::NeuronRandom& _rng,
      FXMVECTOR _center, float _innerR, float _outerR);

    // Color palette for resource types.
    static XMFLOAT4 ResourceColor(uint32_t _resourceType);
  };
}
