#include "pch.h"
#include "CollisionSystem.h"

namespace GameLogic
{
  CollisionSystem::CollisionSystem(SpaceObjectManager& _manager, SpatialHash& _spatialHash)
    : m_manager(_manager)
    , m_spatialHash(_spatialHash)
  {
  }

  void CollisionSystem::RebuildSpatialHash()
  {
    m_spatialHash.Clear();

    m_manager.ForEachActive([&](Neuron::SpaceObject& _obj)
    {
      m_spatialHash.Insert(_obj.Handle, XMLoadFloat3(&_obj.Position));
    });
  }

  void CollisionSystem::DetectCollisions()
  {
    m_collisions.clear();

    // Track tested pairs to avoid duplicates.
    std::unordered_set<uint64_t> testedPairs;

    m_manager.ForEachActive([&](Neuron::SpaceObject& _objA)
    {
      // Skip categories that don't collide (decorations).
      if (_objA.Category == Neuron::SpaceObjectCategory::Decoration)
        return;

      XMVECTOR posA = XMLoadFloat3(&_objA.Position);
      float radiusA = _objA.BoundingRadius;

      // Query the spatial hash for nearby entities.
      m_spatialHash.QuerySphere(posA, radiusA + SpatialHash::CELL_SIZE, [&](Neuron::EntityHandle _handleB)
      {
        if (_objA.Handle == _handleB)
          return; // Skip self

        // Create a canonical pair key to avoid testing A-B and B-A.
        uint32_t idA = _objA.Handle.m_id;
        uint32_t idB = _handleB.m_id;
        uint64_t pairKey = (idA < idB)
          ? (static_cast<uint64_t>(idA) << 32 | idB)
          : (static_cast<uint64_t>(idB) << 32 | idA);

        if (!testedPairs.insert(pairKey).second)
          return; // Already tested this pair.

        Neuron::SpaceObject* objB = m_manager.GetSpaceObject(_handleB);
        if (!objB || !objB->Active)
          return;
        if (objB->Category == Neuron::SpaceObjectCategory::Decoration)
          return;

        XMVECTOR posB = XMLoadFloat3(&objB->Position);
        float radiusB = objB->BoundingRadius;

        if (SphereSphereTest(posA, radiusA, posB, radiusB))
        {
          m_collisions.push_back({ _objA.Handle, _handleB });
        }
      });
    });
  }

  bool CollisionSystem::SphereSphereTest(
    FXMVECTOR _posA, float _radiusA,
    FXMVECTOR _posB, float _radiusB)
  {
    XMVECTOR delta = XMVectorSubtract(_posA, _posB);
    float distSq = XMVectorGetX(XMVector3LengthSq(delta));
    float combined = _radiusA + _radiusB;
    return distSq <= (combined * combined);
  }
}
