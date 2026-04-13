#include "pch.h"
#include "ZoneLoader.h"
#include "GameTypes/ObjectDefs.h"
#include "GameTypes/SurfaceType.h"
#include "GameTypes/PlanetData.h"
#include "GameTypes/SunData.h"
#include "UniverseGenerator.h"
#include "UniversePersistence.h"

namespace EarthRise
{
  bool ZoneLoader::LoadFromFile(Zone& _zone, const std::wstring& _filePath)
  {
    byte_buffer_t buffer = BinaryFile::ReadFile(_filePath);
    if (buffer.empty())
      return false;

    Neuron::BinaryDataReader reader(buffer);

    uint32_t entityCount = reader.Read<uint32_t>();

    for (uint32_t i = 0; i < entityCount; ++i)
    {
      auto category      = reader.Read<Neuron::SpaceObjectCategory>();
      uint16_t defIndex   = reader.Read<uint16_t>();
      uint32_t meshHash   = reader.Read<uint32_t>();
      auto position       = reader.Read<XMFLOAT3>();
      auto orientation    = reader.Read<XMFLOAT4>();
      auto color          = reader.Read<XMFLOAT4>();
      float boundingRadius = reader.Read<float>();

      Neuron::EntityHandle handle = _zone.GetEntityManager().CreateEntity(
        category, position, meshHash, defIndex);

      if (handle.IsValid())
      {
        Neuron::SpaceObject* obj = _zone.GetEntityManager().GetSpaceObject(handle);
        if (obj)
        {
          obj->Orientation    = orientation;
          obj->Color          = color;
          obj->BoundingRadius = boundingRadius;
        }

        // Set category-specific data from definition
        if (category == Neuron::SpaceObjectCategory::Jumpgate)
        {
          Neuron::JumpgateData* gate = _zone.GetEntityManager().GetJumpgateData(handle);
          if (gate)
          {
            gate->ActivationRadius     = reader.Read<float>();
            gate->DestinationPosition  = reader.Read<XMFLOAT3>();
          }
        }
      }
    }

    Neuron::Server::ServerLog("ZoneLoader: Loaded {} entities from zone file\n", entityCount);
    return true;
  }

  void ZoneLoader::CreateTestZone(Zone& _zone)
  {
    auto& mgr = _zone.GetEntityManager();

    // Spawn some test asteroids
    Neuron::NeuronRandom rng(42);
    for (int i = 0; i < 20; ++i)
    {
      float x = static_cast<float>(i * 8 - 80);
      float y = static_cast<float>((i % 5) * 4 - 10);
      float z = 20.0f + static_cast<float>(i * 2);
      auto h = mgr.CreateEntity(Neuron::SpaceObjectCategory::Asteroid,
        XMFLOAT3{ x, y, z }, Neuron::HashMeshName("Asteroid01"));
      if (auto* obj = mgr.GetSpaceObject(h))
      {
        obj->BoundingRadius = 1.2f;
        obj->Color = { 0.6f, 0.5f, 0.2f, 1.0f };
        obj->Surface = static_cast<Neuron::SurfaceType>(i % static_cast<int>(Neuron::SurfaceType::COUNT));
      }
      if (auto* ad = mgr.GetAsteroidData(h))
      {
        ad->RotationSpeed = rng.FloatRange(0.05f, 0.5f);
        ad->RotationAxis = rng.UnitSphere();
      }
    }

    // Spawn a station
    {
      auto h = mgr.CreateEntity(Neuron::SpaceObjectCategory::Station,
        XMFLOAT3{ 0.0f, 0.0f, 40.0f }, Neuron::HashMeshName("Mining01"));
      if (auto* obj = mgr.GetSpaceObject(h))
      {
        obj->BoundingRadius = 8.0f;
        obj->Color = { 0.7f, 0.0f, 0.8f, 1.0f };
      }
      if (auto* sd = mgr.GetStationData(h))
      {
        sd->DockingRadius = 12.0f;
        sd->HasRepair = true;
        sd->HasMarket = true;
      }
    }

    // Spawn two jumpgates forming a pair
    Neuron::EntityHandle gate1, gate2;
    {
      gate1 = mgr.CreateEntity(Neuron::SpaceObjectCategory::Jumpgate,
        XMFLOAT3{ -200.0f, 0.0f, 0.0f }, Neuron::HashMeshName("Jumpgate01"));
      gate2 = mgr.CreateEntity(Neuron::SpaceObjectCategory::Jumpgate,
        XMFLOAT3{ 200.0f, 0.0f, 0.0f }, Neuron::HashMeshName("Jumpgate01"));

      if (auto* obj = mgr.GetSpaceObject(gate1))
      {
        obj->BoundingRadius = 2.0f;
        obj->Color = { 0.0f, 1.0f, 0.8f, 1.0f };
      }
      if (auto* gd = mgr.GetJumpgateData(gate1))
      {
        gd->ActivationRadius = 4.0f;
        gd->DestinationPosition = { 204.0f, 0.0f, 0.0f }; // Near gate2
      }

      if (auto* obj = mgr.GetSpaceObject(gate2))
      {
        obj->BoundingRadius = 2.0f;
        obj->Color = { 0.0f, 1.0f, 0.8f, 1.0f };
      }
      if (auto* gd = mgr.GetJumpgateData(gate2))
      {
        gd->ActivationRadius = 4.0f;
        gd->DestinationPosition = { -196.0f, 0.0f, 0.0f }; // Near gate1
      }
    }

    // Spawn celestial bodies (camera-locked on client; positions define direction only)
    {
      // Sun — center of the system, far above the action plane
      auto sunHandle = mgr.CreateEntity(Neuron::SpaceObjectCategory::Sun,
        XMFLOAT3{ 0.0f, 5000.0f, 40000.0f }, 0); // MeshHash 0: billboard, no .cmo
      if (auto* obj = mgr.GetSpaceObject(sunHandle))
      {
        obj->Color = { 1.0f, 0.85f, 0.4f, 1.0f };
        obj->BoundingRadius = 500.0f;
      }
      if (auto* sd = mgr.GetSunData(sunHandle))
      {
        sd->VisualRadius = 800.0f;
      }

      // Planet 1 — rocky/desert, upper-left of sky
      auto planet1 = mgr.CreateEntity(Neuron::SpaceObjectCategory::Planet,
        XMFLOAT3{ -30000.0f, 8000.0f, 25000.0f }, Neuron::HashMeshName("Planet01"));
      if (auto* obj = mgr.GetSpaceObject(planet1))
      {
        obj->Color = { 0.8f, 0.6f, 0.4f, 1.0f };
        obj->BoundingRadius = 300.0f;
        obj->Surface = Neuron::SurfaceType::Desert;
      }

      // Planet 2 — icy, lower-right of sky
      auto planet2 = mgr.CreateEntity(Neuron::SpaceObjectCategory::Planet,
        XMFLOAT3{ 20000.0f, -5000.0f, 35000.0f }, Neuron::HashMeshName("Planet02"));
      if (auto* obj = mgr.GetSpaceObject(planet2))
      {
        obj->Color = { 0.5f, 0.7f, 0.9f, 1.0f };
        obj->BoundingRadius = 250.0f;
        obj->Surface = Neuron::SurfaceType::IceCap;
      }
    }

    Neuron::Server::ServerLog("ZoneLoader: Created test zone with {} entities\n", mgr.ActiveCount());
  }

  bool ZoneLoader::LoadUniverse(Zone& _zone, uint64_t _seed, const std::wstring& _deltaFilePath)
  {
    auto& mgr = _zone.GetEntityManager();

    // 1. Generate canonical layout from seed.
    auto layout = GameLogic::UniverseGenerator::Generate(_seed);

    // Store universe metadata in zone.
    _zone.SetUniverseSeed(_seed);

    // 2. Spawn celestial bodies (suns and planets).
    for (const auto& body : layout.Bodies)
    {
      auto handle = mgr.CreateEntity(body.Category, body.Position, body.MeshNameHash);
      if (!handle.IsValid())
        continue;

      if (auto* obj = mgr.GetSpaceObject(handle))
      {
        obj->Color          = body.Color;
        obj->BoundingRadius = body.BoundingRadius;
        obj->Surface        = body.Surface;
      }

      if (body.Category == Neuron::SpaceObjectCategory::Planet)
      {
        if (auto* pd = mgr.GetPlanetData(handle))
        {
          pd->OrbitRadius   = body.PlanetInit.OrbitRadius;
          pd->RotationSpeed = body.PlanetInit.RotationSpeed;
          pd->RotationAxis  = body.PlanetInit.RotationAxis;
          pd->HasRings      = body.PlanetInit.HasRings;
        }
      }
      else if (body.Category == Neuron::SpaceObjectCategory::Sun)
      {
        if (auto* sd = mgr.GetSunData(handle))
        {
          sd->Luminosity   = body.SunInit.Luminosity;
          sd->VisualRadius = body.SunInit.VisualRadius;
        }
      }
    }

    // 3. Load delta state (if present) to know which asteroids are depleted.
    std::set<uint32_t> depletedIndices;
    if (!_deltaFilePath.empty())
    {
      GameLogic::UniversePersistence::DeltaState delta;
      if (GameLogic::UniversePersistence::LoadDelta(_deltaFilePath, _seed, delta))
      {
        // Record depleted asteroid indices.
        for (const auto& clusterDelta : delta.ClusterDeltas)
        {
          for (uint32_t idx : clusterDelta.DepletedSpawnIndices)
            depletedIndices.insert(idx);
        }

        // Restore cluster runtime state.
        for (auto& clusterDelta : delta.ClusterDeltas)
        {
          if (clusterDelta.ClusterId < layout.Clusters.size())
          {
            auto& cluster = layout.Clusters[clusterDelta.ClusterId];
            cluster.ActiveCount = clusterDelta.ActiveCount;
            cluster.RegrowthQueue.clear();
            for (float timer : clusterDelta.RegrowthTimers)
              cluster.RegrowthQueue.push_back({ timer });
          }
        }

        Neuron::Server::ServerLog("ZoneLoader: Applied delta state ({} depleted asteroids)\n",
          static_cast<uint32_t>(depletedIndices.size()));
      }
    }

    // 4. Spawn asteroids (skip depleted ones).
    uint32_t spawnedCount = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(layout.Asteroids.size()); ++i)
    {
      if (depletedIndices.contains(i))
        continue;

      const auto& ast = layout.Asteroids[i];
      auto handle = mgr.CreateEntity(Neuron::SpaceObjectCategory::Asteroid,
        ast.Position, ast.MeshNameHash);
      if (!handle.IsValid())
        continue;

      if (auto* obj = mgr.GetSpaceObject(handle))
      {
        obj->Color          = ast.Color;
        obj->BoundingRadius = ast.BoundingRadius;
        obj->Surface        = ast.Surface;
      }

      if (auto* ad = mgr.GetAsteroidData(handle))
      {
        ad->RotationSpeed  = ast.RotationSpeed;
        ad->RotationAxis   = ast.RotationAxis;
        ad->ResourceType   = ast.ResourceType;
        ad->ResourceAmount = ast.ResourceAmount;
        ad->ClusterId      = ast.ClusterId;
        ad->MaxResource    = ast.ResourceAmount;
      }

      ++spawnedCount;
    }

    // 5. Transfer cluster data to zone.
    _zone.SetClusters(std::move(layout.Clusters));

    Neuron::Server::ServerLog("ZoneLoader: Universe loaded — {} bodies, {} asteroids (seed={})\n",
      static_cast<uint32_t>(layout.Bodies.size()), spawnedCount, _seed);

    return true;
  }
}
