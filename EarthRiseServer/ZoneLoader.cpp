#include "pch.h"
#include "ZoneLoader.h"
#include "GameTypes/ObjectDefs.h"
#include "GameTypes/SurfaceType.h"
#include "GameTypes/PlanetData.h"
#include "GameTypes/SunData.h"

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
}
