#include "pch.h"
#include "RegrowthSystem.h"
#include "GameTypes/ObjectDefs.h"

namespace GameLogic
{
  // Asteroid mesh variants (same list as UniverseGenerator).
  static constexpr const char* ASTEROID_MESHES[] = {
    "Asteroid01", "Asteroid02", "Asteroid03", "Asteroid04", "Asteroid05"
  };
  static constexpr uint32_t ASTEROID_MESH_COUNT = static_cast<uint32_t>(std::size(ASTEROID_MESHES));

  RegrowthSystem::RegrowthSystem(SpaceObjectManager& _manager,
    std::vector<Neuron::AsteroidCluster>& _clusters)
    : m_manager(_manager)
    , m_clusters(_clusters)
    , m_rng(0)
  {
  }

  void RegrowthSystem::Seed(uint64_t _universeSeed)
  {
    m_rng.Seed(_universeSeed + 0x52454752); // 'REGR' — distinct sub-seed
  }

  void RegrowthSystem::EnqueueDepletions(const MiningSystem& _mining)
  {
    for (const auto& depletion : _mining.GetDepletions())
    {
      if (depletion.ClusterId < m_clusters.size())
      {
        auto& cluster = m_clusters[depletion.ClusterId];
        if (cluster.ActiveCount > 0)
          --cluster.ActiveCount;

        cluster.RegrowthQueue.push_back({ cluster.RespawnInterval });
      }
    }
  }

  void RegrowthSystem::Update(float _deltaTime)
  {
    for (auto& cluster : m_clusters)
    {
      // Tick all regrowth timers.
      for (auto it = cluster.RegrowthQueue.begin(); it != cluster.RegrowthQueue.end(); )
      {
        it->Timer -= _deltaTime;

        if (it->Timer <= 0.0f)
        {
          // Respawn if under cap.
          if (cluster.ActiveCount < cluster.AsteroidCount)
          {
            XMFLOAT3 pos = GenerateSpawnPosition(cluster);

            uint32_t meshIdx = m_rng.UintRange(0, ASTEROID_MESH_COUNT - 1);
            auto handle = m_manager.CreateEntity(
              Neuron::SpaceObjectCategory::Asteroid,
              pos,
              Neuron::HashMeshName(ASTEROID_MESHES[meshIdx]));

            if (handle.IsValid())
            {
              if (auto* obj = m_manager.GetSpaceObject(handle))
              {
                obj->BoundingRadius = m_rng.FloatRange(cluster.MinRadius, cluster.MaxRadius);
                obj->Color = { 0.5f, 0.4f, 0.3f, 1.0f }; // Default rock color
              }

              if (auto* ad = m_manager.GetAsteroidData(handle))
              {
                ad->RotationSpeed  = m_rng.FloatRange(0.05f, 0.5f);
                ad->RotationAxis   = m_rng.UnitSphere();
                ad->ResourceType   = cluster.ResourceType;
                ad->ResourceAmount = cluster.MaxResource;
                ad->ClusterId      = cluster.ClusterId;
                ad->MaxResource    = cluster.MaxResource;
              }

              ++cluster.ActiveCount;
            }
          }

          it = cluster.RegrowthQueue.erase(it);
        }
        else
        {
          ++it;
        }
      }
    }
  }

  XMFLOAT3 RegrowthSystem::GenerateSpawnPosition(const Neuron::AsteroidCluster& _cluster)
  {
    XMVECTOR center = XMLoadFloat3(&_cluster.Center);
    XMVECTOR orientation = XMLoadFloat4(&_cluster.Orientation);

    XMFLOAT3 result;

    if (_cluster.Shape == Neuron::ClusterShape::Ring)
    {
      // Torus sampling.
      float theta = m_rng.Angle();
      float phi = m_rng.Angle();
      float R = m_rng.FloatRange(_cluster.InnerRadius, _cluster.OuterRadius);
      float r = m_rng.FloatRange(0.0f, _cluster.Thickness * 0.5f);

      float x = (R + r * std::cosf(phi)) * std::cosf(theta);
      float y = r * std::sinf(phi);
      float z = (R + r * std::cosf(phi)) * std::sinf(theta);

      XMVECTOR localPos = XMVectorSet(x, y, z, 0.0f);
      localPos = XMVector3Rotate(localPos, orientation);
      XMStoreFloat3(&result, XMVectorAdd(localPos, center));
    }
    else
    {
      // Supernova cloud sampling (inverse-square distribution).
      XMFLOAT3 dir = m_rng.UnitSphere();
      XMVECTOR direction = XMLoadFloat3(&dir);

      float t = m_rng.Unit();
      float distance = _cluster.InnerRadius +
        (_cluster.OuterRadius - _cluster.InnerRadius) * (1.0f - std::sqrtf(t));

      XMVECTOR point = XMVectorAdd(center, XMVectorScale(direction, distance));
      XMStoreFloat3(&result, point);
    }

    return result;
  }
}
