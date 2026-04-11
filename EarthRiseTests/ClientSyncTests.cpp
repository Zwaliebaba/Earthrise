#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Neuron;

// =========================================================================
// ClientEntity — lightweight copy of the client-side entity for testing.
// We redefine the struct here instead of including GameApp's header
// (which depends on GameRender DX12 types unavailable in the test DLL).
// =========================================================================
namespace
{
  struct TestClientEntity
  {
    EntityHandle Handle;
    SpaceObjectCategory Category = SpaceObjectCategory::SpaceObject;
    uint32_t MeshHash = 0;
    bool Active = false;

    XMFLOAT3 Position = {};
    XMFLOAT4 Orientation = { 0, 0, 0, 1 };
    XMFLOAT4 Color = { 1, 1, 1, 1 };
    XMFLOAT3 Velocity = {};

    XMFLOAT3 PrevPosition = {};
    XMFLOAT4 PrevOrientation = { 0, 0, 0, 1 };
    XMFLOAT3 TargetPosition = {};
    XMFLOAT4 TargetOrientation = { 0, 0, 0, 1 };

    std::string MeshKey;
  };

  // Simplified world state for testing interpolation and prediction logic
  class TestWorldState
  {
  public:
    void ApplySpawn(const EntitySpawnMsg& _msg)
    {
      TestClientEntity entity;
      entity.Handle = _msg.Handle;
      entity.Category = _msg.Category;
      entity.MeshHash = _msg.MeshHash;
      entity.Active = true;
      entity.Position = _msg.Position;
      entity.Orientation = _msg.Orientation;
      entity.Color = _msg.Color;
      entity.PrevPosition = _msg.Position;
      entity.PrevOrientation = _msg.Orientation;
      entity.TargetPosition = _msg.Position;
      entity.TargetOrientation = _msg.Orientation;
      m_entities[_msg.Handle.m_id] = std::move(entity);
    }

    void ApplyDespawn(const EntityDespawnMsg& _msg)
    {
      m_entities.erase(_msg.Handle.m_id);
    }

    void ApplySnapshot(const StateSnapshotMsg& _msg)
    {
      for (const auto& state : _msg.Entities)
      {
        auto it = m_entities.find(state.HandleId);
        if (it == m_entities.end()) continue;
        auto& e = it->second;
        e.PrevPosition = e.TargetPosition;
        e.PrevOrientation = e.TargetOrientation;
        e.TargetPosition = state.Position;
        e.TargetOrientation = state.Orientation;
        e.Velocity = state.Velocity;
      }
    }

    void Interpolate(float _t)
    {
      for (auto& [id, e] : m_entities)
      {
        if (!e.Active) continue;
        XMVECTOR prev = XMLoadFloat3(&e.PrevPosition);
        XMVECTOR target = XMLoadFloat3(&e.TargetPosition);
        XMVECTOR pos = XMVectorLerp(prev, target, _t);
        XMStoreFloat3(&e.Position, pos);

        XMVECTOR prevOri = XMLoadFloat4(&e.PrevOrientation);
        XMVECTOR targetOri = XMLoadFloat4(&e.TargetOrientation);
        XMVECTOR ori = XMQuaternionSlerp(prevOri, targetOri, _t);
        XMStoreFloat4(&e.Orientation, ori);
      }
    }

    const TestClientEntity* GetEntity(EntityHandle h) const
    {
      auto it = m_entities.find(h.m_id);
      return (it != m_entities.end() && it->second.Active) ? &it->second : nullptr;
    }

    uint32_t ActiveCount() const
    {
      uint32_t count = 0;
      for (const auto& [id, e] : m_entities)
        if (e.Active) ++count;
      return count;
    }

  private:
    std::unordered_map<uint32_t, TestClientEntity> m_entities;
  };

  // Simplified prediction for testing
  struct TestPredictionEntry
  {
    EntityHandle Handle;
    XMFLOAT3 CommandTarget = {};
    bool HasPendingCommand = false;
    XMFLOAT3 PredictedPosition = {};
  };

  class TestPrediction
  {
  public:
    void ApplyLocalCommand(EntityHandle _h, const XMFLOAT3& _curPos, const XMFLOAT3& _target)
    {
      TestPredictionEntry entry;
      entry.Handle = _h;
      entry.CommandTarget = _target;
      entry.HasPendingCommand = true;
      entry.PredictedPosition = _curPos;
      m_entries[_h.m_id] = entry;
    }

    void Update(float _deltaT, float _speed)
    {
      for (auto& [id, e] : m_entries)
      {
        if (!e.HasPendingCommand) continue;
        XMVECTOR pos = XMLoadFloat3(&e.PredictedPosition);
        XMVECTOR target = XMLoadFloat3(&e.CommandTarget);
        XMVECTOR toTarget = XMVectorSubtract(target, pos);
        float dist = XMVectorGetX(XMVector3Length(toTarget));
        if (dist < 1.0f)
        {
          e.PredictedPosition = e.CommandTarget;
          e.HasPendingCommand = false;
          continue;
        }
        float step = _speed * _deltaT;
        if (step > dist) step = dist;
        XMVECTOR dir = XMVector3Normalize(toTarget);
        XMStoreFloat3(&e.PredictedPosition, XMVectorAdd(pos, XMVectorScale(dir, step)));
      }
    }

    void Reconcile(EntityHandle _h, const XMFLOAT3& _serverPos, float _snapThreshold = 5.0f)
    {
      auto it = m_entries.find(_h.m_id);
      if (it == m_entries.end()) return;
      auto& e = it->second;
      XMVECTOR predicted = XMLoadFloat3(&e.PredictedPosition);
      XMVECTOR server = XMLoadFloat3(&_serverPos);
      float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(predicted, server)));
      if (dist > _snapThreshold)
        e.PredictedPosition = _serverPos;
      else
      {
        XMVECTOR blended = XMVectorLerp(predicted, server, 0.3f);
        XMStoreFloat3(&e.PredictedPosition, blended);
      }
    }

    const XMFLOAT3* GetPredictedPosition(EntityHandle _h) const
    {
      auto it = m_entries.find(_h.m_id);
      if (it != m_entries.end() && it->second.HasPendingCommand)
        return &it->second.PredictedPosition;
      return nullptr;
    }

    bool HasPrediction(EntityHandle _h) const
    {
      auto it = m_entries.find(_h.m_id);
      return it != m_entries.end() && it->second.HasPendingCommand;
    }

  private:
    std::unordered_map<uint32_t, TestPredictionEntry> m_entries;
  };
}

namespace EarthRiseTests::ClientSync
{
  // =====================================================================
  // Interpolation Tests
  // =====================================================================
  TEST_CLASS(InterpolationTests)
  {
  public:
    TEST_METHOD(LerpPositionAtMidpoint)
    {
      XMFLOAT3 a = { 0, 0, 0 };
      XMFLOAT3 b = { 10, 20, 30 };
      XMVECTOR result = XMVectorLerp(XMLoadFloat3(&a), XMLoadFloat3(&b), 0.5f);
      XMFLOAT3 r;
      XMStoreFloat3(&r, result);
      Assert::AreEqual(5.0f, r.x, 0.01f);
      Assert::AreEqual(10.0f, r.y, 0.01f);
      Assert::AreEqual(15.0f, r.z, 0.01f);
    }

    TEST_METHOD(LerpPositionAtZero)
    {
      XMFLOAT3 a = { 1, 2, 3 };
      XMFLOAT3 b = { 10, 20, 30 };
      XMVECTOR result = XMVectorLerp(XMLoadFloat3(&a), XMLoadFloat3(&b), 0.0f);
      XMFLOAT3 r;
      XMStoreFloat3(&r, result);
      Assert::AreEqual(1.0f, r.x, 0.01f);
      Assert::AreEqual(2.0f, r.y, 0.01f);
      Assert::AreEqual(3.0f, r.z, 0.01f);
    }

    TEST_METHOD(LerpPositionAtOne)
    {
      XMFLOAT3 a = { 1, 2, 3 };
      XMFLOAT3 b = { 10, 20, 30 };
      XMVECTOR result = XMVectorLerp(XMLoadFloat3(&a), XMLoadFloat3(&b), 1.0f);
      XMFLOAT3 r;
      XMStoreFloat3(&r, result);
      Assert::AreEqual(10.0f, r.x, 0.01f);
      Assert::AreEqual(20.0f, r.y, 0.01f);
      Assert::AreEqual(30.0f, r.z, 0.01f);
    }

    TEST_METHOD(SlerpOrientationAtMidpoint)
    {
      // Identity to 90-degree rotation around Y
      XMVECTOR a = XMQuaternionIdentity();
      XMVECTOR b = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), XM_PIDIV2);
      XMVECTOR result = XMQuaternionSlerp(a, b, 0.5f);

      // At midpoint, should be 45-degree rotation
      XMVECTOR expected = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), XM_PIDIV4);
      XMVECTOR diff = XMVectorSubtract(result, expected);
      float len = XMVectorGetX(XMVector4Length(diff));
      Assert::IsTrue(len < 0.01f, L"Slerp at midpoint should be halfway rotation");
    }

    TEST_METHOD(ExtrapolateUsingVelocity)
    {
      XMFLOAT3 pos = { 10, 0, 0 };
      XMFLOAT3 vel = { 5, 0, 0 };
      float dt = 2.0f;
      XMVECTOR result = XMVectorAdd(XMLoadFloat3(&pos), XMVectorScale(XMLoadFloat3(&vel), dt));
      XMFLOAT3 r;
      XMStoreFloat3(&r, result);
      Assert::AreEqual(20.0f, r.x, 0.01f);
    }

    TEST_METHOD(WorldStateInterpolationAtHalf)
    {
      TestWorldState world;

      EntitySpawnMsg spawn;
      spawn.Handle = EntityHandle(1, 1);
      spawn.Category = SpaceObjectCategory::Asteroid;
      spawn.Position = { 0, 0, 0 };
      spawn.Orientation = { 0, 0, 0, 1 };
      spawn.Color = { 1, 1, 1, 1 };
      world.ApplySpawn(spawn);

      // Simulate snapshot moving entity
      StateSnapshotMsg snapshot;
      snapshot.ServerTick = 1;
      EntityState es;
      es.HandleId = spawn.Handle.m_id;
      es.Position = { 100, 0, 0 };
      es.Orientation = { 0, 0, 0, 1 };
      es.Velocity = { 0, 0, 0 };
      snapshot.Entities.push_back(es);
      world.ApplySnapshot(snapshot);

      // Interpolate at 0.5
      world.Interpolate(0.5f);

      const auto* entity = world.GetEntity(spawn.Handle);
      Assert::IsNotNull(entity);
      Assert::AreEqual(50.0f, entity->Position.x, 0.01f);
    }
  };

  // =====================================================================
  // Prediction Tests
  // =====================================================================
  TEST_CLASS(PredictionTests)
  {
  public:
    TEST_METHOD(ApplyLocalCommandCreatesPrediction)
    {
      TestPrediction pred;
      EntityHandle h(1, 1);
      XMFLOAT3 pos = { 0, 0, 0 };
      XMFLOAT3 target = { 100, 0, 0 };
      pred.ApplyLocalCommand(h, pos, target);
      Assert::IsTrue(pred.HasPrediction(h));
    }

    TEST_METHOD(UpdateMovesPredictedPosition)
    {
      TestPrediction pred;
      EntityHandle h(1, 1);
      XMFLOAT3 pos = { 0, 0, 0 };
      XMFLOAT3 target = { 100, 0, 0 };
      pred.ApplyLocalCommand(h, pos, target);

      pred.Update(1.0f, 50.0f);

      const XMFLOAT3* predicted = pred.GetPredictedPosition(h);
      Assert::IsNotNull(predicted);
      Assert::AreEqual(50.0f, predicted->x, 0.5f);
    }

    TEST_METHOD(ReconcileSnapsToServerWhenFar)
    {
      TestPrediction pred;
      EntityHandle h(1, 1);
      XMFLOAT3 pos = { 0, 0, 0 };
      XMFLOAT3 target = { 100, 0, 0 };
      pred.ApplyLocalCommand(h, pos, target);

      // Server says entity is at (50, 0, 0) — far from (0, 0, 0)
      pred.Reconcile(h, { 50, 0, 0 }, 5.0f);

      const XMFLOAT3* predicted = pred.GetPredictedPosition(h);
      Assert::IsNotNull(predicted);
      // Should snap to server position
      Assert::AreEqual(50.0f, predicted->x, 0.01f);
    }

    TEST_METHOD(ReconcileBlendsWhenClose)
    {
      TestPrediction pred;
      EntityHandle h(1, 1);
      XMFLOAT3 pos = { 48, 0, 0 };
      XMFLOAT3 target = { 100, 0, 0 };
      pred.ApplyLocalCommand(h, pos, target);

      // Server says (50, 0, 0) — close to (48, 0, 0)
      pred.Reconcile(h, { 50, 0, 0 }, 5.0f);

      const XMFLOAT3* predicted = pred.GetPredictedPosition(h);
      Assert::IsNotNull(predicted);
      // Blended: 48 * 0.7 + 50 * 0.3 = 48.6
      Assert::AreEqual(48.6f, predicted->x, 0.1f);
    }

    TEST_METHOD(ArrivalClearsPrediction)
    {
      TestPrediction pred;
      EntityHandle h(1, 1);
      XMFLOAT3 pos = { 99.5f, 0, 0 };
      XMFLOAT3 target = { 100, 0, 0 };
      pred.ApplyLocalCommand(h, pos, target);

      pred.Update(1.0f, 100.0f);

      // Should have arrived (within 1.0 unit)
      Assert::IsFalse(pred.HasPrediction(h));
    }
  };

  // =====================================================================
  // ClientWorldState Tests (spawn/despawn)
  // =====================================================================
  TEST_CLASS(ClientWorldStateTests)
  {
  public:
    TEST_METHOD(SpawnCreatesEntity)
    {
      TestWorldState world;

      EntitySpawnMsg spawn;
      spawn.Handle = EntityHandle(1, 1);
      spawn.Category = SpaceObjectCategory::Ship;
      spawn.Position = { 10, 20, 30 };
      spawn.Orientation = { 0, 0, 0, 1 };
      spawn.Color = { 0, 1, 0, 1 };
      world.ApplySpawn(spawn);

      Assert::AreEqual(uint32_t(1), world.ActiveCount());
      const auto* e = world.GetEntity(spawn.Handle);
      Assert::IsNotNull(e);
      Assert::AreEqual(10.0f, e->Position.x, 0.01f);
    }

    TEST_METHOD(DespawnRemovesEntity)
    {
      TestWorldState world;

      EntitySpawnMsg spawn;
      spawn.Handle = EntityHandle(1, 1);
      spawn.Category = SpaceObjectCategory::Asteroid;
      spawn.Position = {};
      world.ApplySpawn(spawn);

      EntityDespawnMsg despawn;
      despawn.Handle = spawn.Handle;
      world.ApplyDespawn(despawn);

      Assert::AreEqual(uint32_t(0), world.ActiveCount());
      Assert::IsNull(world.GetEntity(spawn.Handle));
    }

    TEST_METHOD(SnapshotUpdatesTarget)
    {
      TestWorldState world;

      EntitySpawnMsg spawn;
      spawn.Handle = EntityHandle(5, 2);
      spawn.Category = SpaceObjectCategory::Station;
      spawn.Position = { 0, 0, 0 };
      world.ApplySpawn(spawn);

      StateSnapshotMsg snap;
      snap.ServerTick = 42;
      EntityState es;
      es.HandleId = spawn.Handle.m_id;
      es.Position = { 500, 200, -100 };
      es.Orientation = { 0, 0, 0, 1 };
      es.Velocity = { 10, 0, 0 };
      snap.Entities.push_back(es);
      world.ApplySnapshot(snap);

      const auto* e = world.GetEntity(spawn.Handle);
      Assert::IsNotNull(e);
      // Target should be updated
      Assert::AreEqual(500.0f, e->TargetPosition.x, 0.01f);
      Assert::AreEqual(200.0f, e->TargetPosition.y, 0.01f);
    }

    TEST_METHOD(MultipleEntitiesTracked)
    {
      TestWorldState world;

      for (uint32_t i = 1; i <= 5; ++i)
      {
        EntitySpawnMsg spawn;
        spawn.Handle = EntityHandle(i, 1);
        spawn.Category = SpaceObjectCategory::Ship;
        spawn.Position = { static_cast<float>(i * 10), 0, 0 };
        world.ApplySpawn(spawn);
      }

      Assert::AreEqual(uint32_t(5), world.ActiveCount());
    }

    TEST_METHOD(DespawnUnknownHandleIsNoop)
    {
      TestWorldState world;

      EntitySpawnMsg spawn;
      spawn.Handle = EntityHandle(1, 1);
      world.ApplySpawn(spawn);

      EntityDespawnMsg despawn;
      despawn.Handle = EntityHandle(99, 1); // doesn't exist
      world.ApplyDespawn(despawn);

      Assert::AreEqual(uint32_t(1), world.ActiveCount());
    }
  };

  // =====================================================================
  // InputState Tests (CPU-only, no window required)
  // =====================================================================
  TEST_CLASS(InputStateTests)
  {
  public:
    TEST_METHOD(KeyDownTrackedAfterKeydown)
    {
      // Simulate key state tracking without actual window messages
      bool keyDown[256] = {};
      bool prevKeyDown[256] = {};

      // Simulate W key down
      keyDown['W'] = true;
      Assert::IsTrue(keyDown['W']);
      Assert::IsFalse(prevKeyDown['W']);

      // IsKeyPressed: down now, up before
      bool pressed = keyDown['W'] && !prevKeyDown['W'];
      Assert::IsTrue(pressed);

      // Next frame
      std::memcpy(prevKeyDown, keyDown, sizeof(keyDown));
      // IsKeyPressed should be false (held, not newly pressed)
      pressed = keyDown['W'] && !prevKeyDown['W'];
      Assert::IsFalse(pressed);
    }

    TEST_METHOD(KeyReleasedTracked)
    {
      bool keyDown[256] = {};
      bool prevKeyDown[256] = {};

      // Key was down
      keyDown['A'] = true;
      std::memcpy(prevKeyDown, keyDown, sizeof(keyDown));

      // Now released
      keyDown['A'] = false;
      bool released = !keyDown['A'] && prevKeyDown['A'];
      Assert::IsTrue(released);
    }

    TEST_METHOD(MouseDeltaComputation)
    {
      float mouseX = 100.0f;
      float mouseY = 200.0f;
      float prevX = 90.0f;
      float prevY = 195.0f;

      float deltaX = mouseX - prevX;
      float deltaY = mouseY - prevY;

      Assert::AreEqual(10.0f, deltaX, 0.01f);
      Assert::AreEqual(5.0f, deltaY, 0.01f);
    }
  };

  // =====================================================================
  // CommandTargeting Tests — 3D ray-cast math
  // =====================================================================
  TEST_CLASS(CommandTargetingTests)
  {
  public:
    TEST_METHOD(ScreenToRayMath)
    {
      // Test the NDC conversion math:
      // Screen center (960, 540) on 1920x1080 → NDC (0, 0)
      float screenX = 960.0f;
      float screenY = 540.0f;
      float screenW = 1920.0f;
      float screenH = 1080.0f;

      float ndcX = (2.0f * screenX / screenW) - 1.0f;
      float ndcY = 1.0f - (2.0f * screenY / screenH);

      Assert::AreEqual(0.0f, ndcX, 0.001f);
      Assert::AreEqual(0.0f, ndcY, 0.001f);
    }

    TEST_METHOD(ScreenToRayTopLeft)
    {
      float screenX = 0.0f;
      float screenY = 0.0f;
      float screenW = 1920.0f;
      float screenH = 1080.0f;

      float ndcX = (2.0f * screenX / screenW) - 1.0f;
      float ndcY = 1.0f - (2.0f * screenY / screenH);

      Assert::AreEqual(-1.0f, ndcX, 0.001f);
      Assert::AreEqual(1.0f, ndcY, 0.001f);
    }

    TEST_METHOD(ScreenToRayBottomRight)
    {
      float screenX = 1920.0f;
      float screenY = 1080.0f;
      float screenW = 1920.0f;
      float screenH = 1080.0f;

      float ndcX = (2.0f * screenX / screenW) - 1.0f;
      float ndcY = 1.0f - (2.0f * screenY / screenH);

      Assert::AreEqual(1.0f, ndcX, 0.001f);
      Assert::AreEqual(-1.0f, ndcY, 0.001f);
    }

    TEST_METHOD(RayPlaneIntersection)
    {
      // Ray from (0, 10, 0) downward hitting Y=0 plane
      XMFLOAT3 origin = { 0, 10, 0 };
      XMFLOAT3 dir = { 0, -1, 0 };
      float planeY = 0.0f;

      float t = (planeY - origin.y) / dir.y;
      Assert::AreEqual(10.0f, t, 0.01f);

      XMFLOAT3 hit;
      hit.x = origin.x + dir.x * t;
      hit.y = origin.y + dir.y * t;
      hit.z = origin.z + dir.z * t;

      Assert::AreEqual(0.0f, hit.x, 0.01f);
      Assert::AreEqual(0.0f, hit.y, 0.01f);
      Assert::AreEqual(0.0f, hit.z, 0.01f);
    }

    TEST_METHOD(RayPlaneIntersectionAngled)
    {
      // Ray from (0, 10, 0) at 45 degrees hitting Y=0 plane
      XMFLOAT3 origin = { 0, 10, 0 };
      float invSqrt2 = 1.0f / std::sqrt(2.0f);
      XMFLOAT3 dir = { 0, -invSqrt2, invSqrt2 };
      float planeY = 0.0f;

      float t = (planeY - origin.y) / dir.y;
      Assert::IsTrue(t > 0.0f);

      XMFLOAT3 hit;
      hit.x = origin.x + dir.x * t;
      hit.y = origin.y + dir.y * t;
      hit.z = origin.z + dir.z * t;

      Assert::AreEqual(0.0f, hit.y, 0.01f);
      Assert::AreEqual(10.0f, hit.z, 0.5f); // Should be ~10 units forward
    }

    TEST_METHOD(RaySphereIntersection)
    {
      // Ray from origin along +Z, sphere at (0, 0, 10) with radius 2
      XMVECTOR rayOrigin = XMVectorSet(0, 0, 0, 0);
      XMVECTOR rayDir = XMVectorSet(0, 0, 1, 0);
      XMVECTOR sphereCenter = XMVectorSet(0, 0, 10, 0);

      XMVECTOR toSphere = XMVectorSubtract(sphereCenter, rayOrigin);
      float along = XMVectorGetX(XMVector3Dot(toSphere, rayDir));
      Assert::AreEqual(10.0f, along, 0.01f);

      XMVECTOR closestOnRay = XMVectorAdd(rayOrigin, XMVectorScale(rayDir, along));
      float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(sphereCenter, closestOnRay)));

      Assert::AreEqual(0.0f, dist, 0.01f); // Direct hit
    }

    TEST_METHOD(RaySphereIntersectionMiss)
    {
      // Ray from origin along +Z, sphere at (10, 0, 10) with radius 2
      XMVECTOR rayOrigin = XMVectorSet(0, 0, 0, 0);
      XMVECTOR rayDir = XMVectorSet(0, 0, 1, 0);
      XMVECTOR sphereCenter = XMVectorSet(10, 0, 10, 0);

      XMVECTOR toSphere = XMVectorSubtract(sphereCenter, rayOrigin);
      float along = XMVectorGetX(XMVector3Dot(toSphere, rayDir));

      XMVECTOR closestOnRay = XMVectorAdd(rayOrigin, XMVectorScale(rayDir, along));
      float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(sphereCenter, closestOnRay)));

      Assert::IsTrue(dist > 2.0f, L"Should miss sphere of radius 2");
    }
  };

  // =====================================================================
  // ComputeAlpha Tests
  // =====================================================================
  TEST_CLASS(ComputeAlphaTests)
  {
  public:
    TEST_METHOD(AlphaAtZero)
    {
      float snapshotInterval = 1.0f / 20.0f;
      float elapsed = 0.0f;
      float alpha = elapsed / snapshotInterval;
      Assert::AreEqual(0.0f, alpha, 0.001f);
    }

    TEST_METHOD(AlphaAtHalf)
    {
      float snapshotInterval = 1.0f / 20.0f;
      float elapsed = snapshotInterval * 0.5f;
      float alpha = elapsed / snapshotInterval;
      Assert::AreEqual(0.5f, alpha, 0.001f);
    }

    TEST_METHOD(AlphaAtOne)
    {
      float snapshotInterval = 1.0f / 20.0f;
      float elapsed = snapshotInterval;
      float alpha = elapsed / snapshotInterval;
      Assert::AreEqual(1.0f, alpha, 0.001f);
    }

    TEST_METHOD(AlphaClampedAtMax)
    {
      float snapshotInterval = 1.0f / 20.0f;
      float elapsed = snapshotInterval * 5.0f; // way past
      float maxAlpha = 2.0f;
      float alpha = elapsed / snapshotInterval;
      if (alpha > maxAlpha) alpha = maxAlpha;
      Assert::AreEqual(2.0f, alpha, 0.001f);
    }
  };
}
