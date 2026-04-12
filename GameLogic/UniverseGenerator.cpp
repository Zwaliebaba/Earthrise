#include "pch.h"
#include "UniverseGenerator.h"
#include "GameTypes/ObjectDefs.h"

namespace GameLogic
{
  // Asteroid mesh variants — cycled based on seed.
  static constexpr const char* ASTEROID_MESHES[] = {
    "Asteroid01", "Asteroid02", "Asteroid03", "Asteroid04", "Asteroid05"
  };
  static constexpr uint32_t ASTEROID_MESH_COUNT = static_cast<uint32_t>(std::size(ASTEROID_MESHES));

  // Resource type colors (tinted by resource).
  XMFLOAT4 UniverseGenerator::ResourceColor(uint32_t _resourceType)
  {
    switch (_resourceType)
    {
    case 1:  return { 0.8f, 0.5f, 0.2f, 1.0f }; // Copper/orange
    case 2:  return { 0.6f, 0.6f, 0.7f, 1.0f }; // Silver/grey
    case 3:  return { 0.9f, 0.8f, 0.1f, 1.0f }; // Gold
    case 4:  return { 0.3f, 0.7f, 0.9f, 1.0f }; // Crystal/blue
    default: return { 0.5f, 0.4f, 0.3f, 1.0f }; // Rock/brown
    }
  }

  // Sun color palette.
  static XMFLOAT4 SunColor(Neuron::NeuronRandom& _rng)
  {
    float r = _rng.FloatRange(0.9f, 1.0f);
    float g = _rng.FloatRange(0.6f, 0.9f);
    float b = _rng.FloatRange(0.2f, 0.5f);
    return { r, g, b, 1.0f };
  }

  // Planet surface types assigned round-robin with some randomness.
  static Neuron::SurfaceType RandomSurface(Neuron::NeuronRandom& _rng)
  {
    return static_cast<Neuron::SurfaceType>(
      _rng.IntRange(0, static_cast<int>(Neuron::SurfaceType::COUNT) - 1));
  }

  UniverseGenerator::UniverseLayout UniverseGenerator::Generate(uint64_t _seed)
  {
    Neuron::NeuronRandom rng(_seed);
    UniverseLayout layout;
    layout.Seed = _seed;

    GenerateSuns(rng, layout);
    GeneratePlanets(rng, layout);
    GenerateRingClusters(rng, layout);
    GenerateSupernovaClusters(rng, layout);
    PopulateClusterAsteroids(rng, layout);

    return layout;
  }

  void UniverseGenerator::GenerateSuns(Neuron::NeuronRandom& _rng, UniverseLayout& _layout)
  {
    int sunCount = _rng.IntRange(1, 3);

    for (int i = 0; i < sunCount; ++i)
    {
      UniverseLayout::CelestialBody sun;
      sun.Category       = Neuron::SpaceObjectCategory::Sun;
      sun.Position       = {
        _rng.FloatRange(-5000.0f, 5000.0f),
        _rng.FloatRange(-2000.0f, 2000.0f),
        _rng.FloatRange(30000.0f, 45000.0f)
      };
      sun.BoundingRadius = _rng.FloatRange(50.0f, 200.0f);
      sun.Color          = SunColor(_rng);
      sun.MeshNameHash   = 0; // Billboard, no .cmo
      sun.SunInit.Luminosity   = _rng.FloatRange(0.8f, 1.5f);
      sun.SunInit.VisualRadius = _rng.FloatRange(500.0f, 1200.0f);

      _layout.Bodies.push_back(sun);
    }
  }

  void UniverseGenerator::GeneratePlanets(Neuron::NeuronRandom& _rng, UniverseLayout& _layout)
  {
    int planetCount = _rng.IntRange(3, 8);

    // Primary sun position (first sun is the orbital center).
    XMFLOAT3 sunPos = _layout.Bodies.empty()
      ? XMFLOAT3{ 0.0f, 0.0f, 40000.0f }
      : _layout.Bodies[0].Position;

    for (int i = 0; i < planetCount; ++i)
    {
      float orbitRadius = _rng.FloatRange(10000.0f, 40000.0f);
      float angle = _rng.Angle();
      float elevation = _rng.FloatRange(-3000.0f, 3000.0f);

      UniverseLayout::CelestialBody planet;
      planet.Category       = Neuron::SpaceObjectCategory::Planet;
      planet.Position       = {
        sunPos.x + orbitRadius * std::cosf(angle),
        sunPos.y + elevation,
        sunPos.z + orbitRadius * std::sinf(angle)
      };
      planet.BoundingRadius = _rng.FloatRange(150.0f, 400.0f);
      planet.Surface        = RandomSurface(_rng);
      planet.Color          = { _rng.FloatRange(0.4f, 1.0f),
                                _rng.FloatRange(0.4f, 1.0f),
                                _rng.FloatRange(0.4f, 1.0f), 1.0f };
      planet.MeshNameHash   = Neuron::HashMeshName("Procedural");

      planet.PlanetInit.OrbitRadius   = orbitRadius;
      planet.PlanetInit.RotationSpeed = _rng.FloatRange(0.01f, 0.1f);
      planet.PlanetInit.HasRings      = (_rng.Unit() < 0.3f); // 30% chance

      XMFLOAT3 axis = _rng.UnitSphere();
      planet.PlanetInit.RotationAxis = axis;

      _layout.Bodies.push_back(planet);
    }
  }

  void UniverseGenerator::GenerateRingClusters(Neuron::NeuronRandom& _rng, UniverseLayout& _layout)
  {
    uint16_t clusterId = 0;

    for (const auto& body : _layout.Bodies)
    {
      if (body.Category != Neuron::SpaceObjectCategory::Planet || !body.PlanetInit.HasRings)
        continue;

      Neuron::AsteroidCluster cluster;
      cluster.ClusterId     = clusterId++;
      cluster.Shape         = Neuron::ClusterShape::Ring;
      cluster.Center        = body.Position;

      // Ring tilt — use planet's rotation axis as orientation quaternion axis.
      float tiltAngle = _rng.FloatRange(0.0f, XM_PIDIV4); // Up to 45° tilt
      XMVECTOR axis = XMLoadFloat3(&body.PlanetInit.RotationAxis);
      XMVECTOR orientQ = XMQuaternionRotationAxis(axis, tiltAngle);
      XMStoreFloat4(&cluster.Orientation, orientQ);

      cluster.InnerRadius   = body.BoundingRadius * 1.2f;
      cluster.OuterRadius   = body.BoundingRadius * _rng.FloatRange(3.0f, 6.0f);
      cluster.Thickness     = body.BoundingRadius * _rng.FloatRange(0.1f, 0.3f);

      cluster.AsteroidCount = static_cast<uint32_t>(_rng.IntRange(50, 150));
      cluster.ResourceType  = _rng.UintRange(1, 4);
      cluster.MinResource   = 50.0f;
      cluster.MaxResource   = 200.0f;
      cluster.MinRadius     = 0.5f;
      cluster.MaxRadius     = 2.5f;
      cluster.RespawnInterval = _rng.FloatRange(240.0f, 360.0f);
      cluster.ActiveCount   = 0;

      _layout.Clusters.push_back(std::move(cluster));
    }
  }

  void UniverseGenerator::GenerateSupernovaClusters(Neuron::NeuronRandom& _rng, UniverseLayout& _layout)
  {
    int supernovaCount = _rng.IntRange(5, 15);
    uint16_t nextClusterId = static_cast<uint16_t>(_layout.Clusters.size());

    for (int i = 0; i < supernovaCount; ++i)
    {
      Neuron::AsteroidCluster cluster;
      cluster.ClusterId     = nextClusterId++;
      cluster.Shape         = Neuron::ClusterShape::Supernova;

      // Place 15–45 km from zone center, random direction.
      float dist = _rng.FloatRange(15000.0f, 45000.0f);
      float angle = _rng.Angle();
      float elev = _rng.FloatRange(-5000.0f, 5000.0f);
      cluster.Center = {
        dist * std::cosf(angle),
        elev,
        dist * std::sinf(angle)
      };

      cluster.InnerRadius   = _rng.FloatRange(0.0f, 200.0f);
      cluster.OuterRadius   = _rng.FloatRange(2000.0f, 8000.0f);
      cluster.Thickness     = 0.0f; // Unused for supernova

      cluster.AsteroidCount = static_cast<uint32_t>(_rng.IntRange(60, 200));
      cluster.ResourceType  = _rng.UintRange(0, 4);
      cluster.MinResource   = 30.0f;
      cluster.MaxResource   = 150.0f;
      cluster.MinRadius     = 0.5f;
      cluster.MaxRadius     = 3.0f;
      cluster.RespawnInterval = _rng.FloatRange(240.0f, 420.0f);
      cluster.ActiveCount   = 0;

      _layout.Clusters.push_back(std::move(cluster));
    }
  }

  void UniverseGenerator::PopulateClusterAsteroids(Neuron::NeuronRandom& _rng, UniverseLayout& _layout)
  {
    for (auto& cluster : _layout.Clusters)
    {
      XMVECTOR center = XMLoadFloat3(&cluster.Center);
      XMVECTOR orientation = XMLoadFloat4(&cluster.Orientation);

      for (uint32_t a = 0; a < cluster.AsteroidCount; ++a)
      {
        UniverseLayout::AsteroidSpawn spawn;

        if (cluster.Shape == Neuron::ClusterShape::Ring)
        {
          spawn.Position = RandomPointInTorus(_rng, center, orientation,
            cluster.InnerRadius, cluster.OuterRadius, cluster.Thickness);
        }
        else
        {
          spawn.Position = RandomPointInSupernovaCloud(_rng, center,
            cluster.InnerRadius, cluster.OuterRadius);
        }

        // Pick mesh variant.
        uint32_t meshIdx = _rng.UintRange(0, ASTEROID_MESH_COUNT - 1);
        spawn.MeshNameHash   = Neuron::HashMeshName(ASTEROID_MESHES[meshIdx]);

        spawn.BoundingRadius = _rng.FloatRange(cluster.MinRadius, cluster.MaxRadius);
        spawn.RotationSpeed  = _rng.FloatRange(0.05f, 0.5f);
        spawn.RotationAxis   = _rng.UnitSphere();
        spawn.ResourceType   = cluster.ResourceType;
        spawn.ResourceAmount = _rng.FloatRange(cluster.MinResource, cluster.MaxResource);
        spawn.ClusterId      = cluster.ClusterId;
        spawn.Color          = ResourceColor(cluster.ResourceType);
        spawn.Surface        = RandomSurface(_rng);

        _layout.Asteroids.push_back(spawn);
      }
    }
  }

  XMFLOAT3 UniverseGenerator::RandomPointInTorus(Neuron::NeuronRandom& _rng,
    FXMVECTOR _center, FXMVECTOR _orientation,
    float _innerR, float _outerR, float _thickness)
  {
    // Major angle (around the ring).
    float theta = _rng.Angle();
    // Tube cross-section angle.
    float phi = _rng.Angle();
    // Major radius (uniform between inner and outer).
    float R = _rng.FloatRange(_innerR, _outerR);
    // Tube radius (uniform).
    float r = _rng.FloatRange(0.0f, _thickness * 0.5f);

    // Point in local torus frame (XZ plane is the ring, Y is up).
    float x = (R + r * std::cosf(phi)) * std::cosf(theta);
    float y = r * std::sinf(phi);
    float z = (R + r * std::cosf(phi)) * std::sinf(theta);

    XMVECTOR localPos = XMVectorSet(x, y, z, 0.0f);

    // Rotate by the cluster orientation quaternion.
    localPos = XMVector3Rotate(localPos, _orientation);

    // Translate by center.
    XMVECTOR result = XMVectorAdd(localPos, _center);

    XMFLOAT3 pos;
    XMStoreFloat3(&pos, result);
    return pos;
  }

  XMFLOAT3 UniverseGenerator::RandomPointInSupernovaCloud(Neuron::NeuronRandom& _rng,
    FXMVECTOR _center, float _innerR, float _outerR)
  {
    // Two sub-populations: 70% core cloud, 30% radial streaks.
    float u = _rng.Unit();

    XMFLOAT3 dir = _rng.UnitSphere();
    XMVECTOR direction = XMLoadFloat3(&dir);

    float distance;
    if (u < 0.7f)
    {
      // Core cloud: inverse-square distribution (denser core).
      float t = _rng.Unit();
      distance = _innerR + (_outerR - _innerR) * (1.0f - std::sqrtf(t));
    }
    else
    {
      // Radial streak: linear distribution along a ray with small cone offset.
      distance = _rng.FloatRange(_innerR, _outerR);

      // Apply small angular offset (5–15° cone).
      float coneAngle = _rng.FloatRange(0.0f, XM_PI / 12.0f); // Up to 15°
      XMFLOAT3 perp = _rng.UnitSphere();
      XMVECTOR perpV = XMLoadFloat3(&perp);
      perpV = XMVector3Cross(direction, perpV);
      float perpLen = Neuron::Math::Length(perpV);
      if (perpLen > 1e-6f)
      {
        perpV = XMVector3Normalize(perpV);
        direction = XMVector3Rotate(direction,
          XMQuaternionRotationAxis(perpV, coneAngle));
      }
    }

    XMVECTOR point = XMVectorAdd(_center, XMVectorScale(direction, distance));

    XMFLOAT3 pos;
    XMStoreFloat3(&pos, point);
    return pos;
  }
}
