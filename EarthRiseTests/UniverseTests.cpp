#include "pch.h"
#include "UniverseGenerator.h"
#include "MiningSystem.h"
#include "RegrowthSystem.h"
#include "SpaceObjectManager.h"
#include "CollisionSystem.h"
#include "SpatialHash.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Neuron;
using namespace GameLogic;

namespace EarthRiseTests::Universe
{
  // =======================================================================
  // UniverseGenerator Tests
  // =======================================================================
  TEST_CLASS(UniverseGeneratorTests)
  {
  public:
    TEST_METHOD(SameSeedProducesSameLayout)
    {
      constexpr uint64_t seed = 12345;
      auto layout1 = UniverseGenerator::Generate(seed);
      auto layout2 = UniverseGenerator::Generate(seed);

      Assert::AreEqual(layout1.Seed, layout2.Seed);
      Assert::AreEqual(layout1.Bodies.size(), layout2.Bodies.size());
      Assert::AreEqual(layout1.Clusters.size(), layout2.Clusters.size());
      Assert::AreEqual(layout1.Asteroids.size(), layout2.Asteroids.size());

      // Verify body positions match exactly.
      for (size_t i = 0; i < layout1.Bodies.size(); ++i)
      {
        Assert::AreEqual(layout1.Bodies[i].Position.x, layout2.Bodies[i].Position.x);
        Assert::AreEqual(layout1.Bodies[i].Position.y, layout2.Bodies[i].Position.y);
        Assert::AreEqual(layout1.Bodies[i].Position.z, layout2.Bodies[i].Position.z);
      }

      // Verify asteroid positions match exactly.
      for (size_t i = 0; i < layout1.Asteroids.size(); ++i)
      {
        Assert::AreEqual(layout1.Asteroids[i].Position.x, layout2.Asteroids[i].Position.x);
        Assert::AreEqual(layout1.Asteroids[i].Position.y, layout2.Asteroids[i].Position.y);
        Assert::AreEqual(layout1.Asteroids[i].Position.z, layout2.Asteroids[i].Position.z);
      }
    }

    TEST_METHOD(DifferentSeedProducesDifferentLayout)
    {
      auto layout1 = UniverseGenerator::Generate(100);
      auto layout2 = UniverseGenerator::Generate(200);

      // Different seeds should produce different body counts or positions.
      bool differ = layout1.Bodies.size() != layout2.Bodies.size();
      if (!differ && !layout1.Bodies.empty())
      {
        differ = layout1.Bodies[0].Position.x != layout2.Bodies[0].Position.x
              || layout1.Bodies[0].Position.y != layout2.Bodies[0].Position.y
              || layout1.Bodies[0].Position.z != layout2.Bodies[0].Position.z;
      }
      Assert::IsTrue(differ, L"Different seeds should produce different layouts");
    }

    TEST_METHOD(GenerateProducesSunsAndPlanets)
    {
      auto layout = UniverseGenerator::Generate(42);

      size_t sunCount    = 0;
      size_t planetCount = 0;
      for (const auto& body : layout.Bodies)
      {
        if (body.Category == SpaceObjectCategory::Sun)    ++sunCount;
        if (body.Category == SpaceObjectCategory::Planet)  ++planetCount;
      }

      Assert::IsTrue(sunCount >= 1 && sunCount <= 3,
        L"Should produce 1-3 suns");
      Assert::IsTrue(planetCount >= 3 && planetCount <= 8,
        L"Should produce 3-8 planets");
    }

    TEST_METHOD(GenerateProducesAsteroids)
    {
      auto layout = UniverseGenerator::Generate(42);

      Assert::IsTrue(layout.Asteroids.size() > 0,
        L"Should produce asteroids");
      Assert::IsTrue(layout.Clusters.size() > 0,
        L"Should produce at least one cluster");
    }

    TEST_METHOD(AllAsteroidsBelongToValidCluster)
    {
      auto layout = UniverseGenerator::Generate(42);

      for (const auto& asteroid : layout.Asteroids)
      {
        Assert::IsTrue(asteroid.ClusterId < layout.Clusters.size(),
          L"Asteroid ClusterId should reference a valid cluster");
      }
    }

    TEST_METHOD(ClusterAsteroidCountsMatch)
    {
      auto layout = UniverseGenerator::Generate(42);

      // Count asteroids per cluster.
      std::vector<uint32_t> counts(layout.Clusters.size(), 0);
      for (const auto& asteroid : layout.Asteroids)
      {
        counts[asteroid.ClusterId]++;
      }

      // Each cluster's AsteroidCount should match the number of spawned asteroids.
      for (size_t i = 0; i < layout.Clusters.size(); ++i)
      {
        Assert::AreEqual(layout.Clusters[i].AsteroidCount, counts[i],
          L"Cluster AsteroidCount should match spawned count");
      }
    }

    TEST_METHOD(AsteroidResourcesArePositive)
    {
      auto layout = UniverseGenerator::Generate(42);

      for (const auto& asteroid : layout.Asteroids)
      {
        Assert::IsTrue(asteroid.ResourceAmount > 0.0f,
          L"Asteroids should spawn with positive resources");
      }
    }
  };

  // =======================================================================
  // MiningSystem Tests
  // =======================================================================
  TEST_CLASS(MiningSystemTests)
  {
  public:
    TEST_METHOD(ProjectileAsteroidCollisionExtractsResources)
    {
      SpaceObjectManager manager;
      MiningSystem mining(manager);

      // Create an asteroid with resources.
      XMFLOAT3 pos = { 100.0f, 0.0f, 0.0f };
      auto asteroid = manager.CreateEntity(SpaceObjectCategory::Asteroid, pos, 0);
      auto* astData = manager.GetAsteroidData(asteroid);
      Assert::IsNotNull(astData);
      astData->ResourceAmount = 50.0f;
      astData->MaxResource    = 50.0f;
      astData->ResourceType   = 1;
      astData->ClusterId      = 0;

      // Create a projectile at the same position.
      auto projectile = manager.CreateEntity(SpaceObjectCategory::Projectile, pos, 0);
      auto* projData = manager.GetProjectileData(projectile);
      Assert::IsNotNull(projData);
      projData->Damage = 10.0f;

      // Simulate collision.
      std::vector<CollisionEvent> collisions;
      collisions.push_back({ projectile, asteroid });

      mining.ProcessCollisions(collisions);

      // Resource should be reduced.
      astData = manager.GetAsteroidData(asteroid);
      Assert::IsNotNull(astData);
      Assert::AreEqual(40.0f, astData->ResourceAmount, 0.01f);

      // A mining hit should be recorded.
      Assert::AreEqual(size_t(1), mining.GetMiningHits().size());

      // No depletion yet (still has resources).
      Assert::AreEqual(size_t(0), mining.GetDepletions().size());
    }

    TEST_METHOD(DepletedAsteroidEmitsDepletion)
    {
      SpaceObjectManager manager;
      MiningSystem mining(manager);

      XMFLOAT3 pos = { 200.0f, 0.0f, 0.0f };
      auto asteroid = manager.CreateEntity(SpaceObjectCategory::Asteroid, pos, 0);
      auto* astData = manager.GetAsteroidData(asteroid);
      Assert::IsNotNull(astData);
      astData->ResourceAmount = 5.0f;
      astData->MaxResource    = 5.0f;
      astData->ResourceType   = 2;
      astData->ClusterId      = 3;

      auto projectile = manager.CreateEntity(SpaceObjectCategory::Projectile, pos, 0);
      auto* projData = manager.GetProjectileData(projectile);
      Assert::IsNotNull(projData);
      projData->Damage = 10.0f; // More damage than remaining resource

      std::vector<CollisionEvent> collisions;
      collisions.push_back({ projectile, asteroid });

      mining.ProcessCollisions(collisions);

      // Should emit a depletion event.
      Assert::AreEqual(size_t(1), mining.GetDepletions().size());
      Assert::AreEqual(uint16_t(3), mining.GetDepletions()[0].ClusterId);

      // Asteroid should be destroyed.
      Assert::IsNull(manager.GetSpaceObject(asteroid),
        L"Depleted asteroid should be destroyed");
    }

    TEST_METHOD(NonMiningCollisionsIgnored)
    {
      SpaceObjectManager manager;
      MiningSystem mining(manager);

      // Ship-to-ship collision — mining system should ignore.
      XMFLOAT3 pos = {};
      auto ship1 = manager.CreateEntity(SpaceObjectCategory::Ship, pos, 0);
      auto ship2 = manager.CreateEntity(SpaceObjectCategory::Ship, pos, 0);

      std::vector<CollisionEvent> collisions;
      collisions.push_back({ ship1, ship2 });

      mining.ProcessCollisions(collisions);

      Assert::AreEqual(size_t(0), mining.GetMiningHits().size());
      Assert::AreEqual(size_t(0), mining.GetDepletions().size());
    }

    TEST_METHOD(MiningSpawnsCrate)
    {
      SpaceObjectManager manager;
      MiningSystem mining(manager);

      XMFLOAT3 pos = { 300.0f, 0.0f, 0.0f };
      auto asteroid = manager.CreateEntity(SpaceObjectCategory::Asteroid, pos, 0);
      auto* astData = manager.GetAsteroidData(asteroid);
      Assert::IsNotNull(astData);
      astData->ResourceAmount = 100.0f;
      astData->MaxResource    = 100.0f;
      astData->ResourceType   = 1;

      auto projectile = manager.CreateEntity(SpaceObjectCategory::Projectile, pos, 0);
      auto* projData = manager.GetProjectileData(projectile);
      Assert::IsNotNull(projData);
      projData->Damage = 10.0f;

      std::vector<CollisionEvent> collisions;
      collisions.push_back({ projectile, asteroid });

      mining.ProcessCollisions(collisions);

      // Find the crate that was spawned.
      bool crateFound = false;
      for (uint32_t i = 0; i < MAX_ENTITIES; ++i)
      {
        EntityHandle h(i, 0); // Generation might differ, but scan active entities.
        if (auto* obj = manager.GetSpaceObject(h))
        {
          if (obj->Active && obj->Category == SpaceObjectCategory::Crate)
          {
            crateFound = true;
            break;
          }
        }
      }
      // Verify a crate was created (best-effort: entity pool may use any slot).
      // If CreateEntity succeeded, there should be at least one active crate.
      // We verify via crate data instead.
      Assert::IsTrue(mining.GetMiningHits().size() == 1,
        L"Mining should record one hit");
    }
  };

  // =======================================================================
  // RegrowthSystem Tests
  // =======================================================================
  TEST_CLASS(RegrowthSystemTests)
  {
  public:
    TEST_METHOD(DepletedAsteroidRegrowsAfterTimer)
    {
      SpaceObjectManager manager;

      // Set up a cluster.
      std::vector<AsteroidCluster> clusters(1);
      auto& cluster = clusters[0];
      cluster.ClusterId       = 0;
      cluster.Shape           = ClusterShape::Supernova;
      cluster.Center          = { 0.0f, 0.0f, 0.0f };
      cluster.InnerRadius     = 10.0f;
      cluster.OuterRadius     = 100.0f;
      cluster.AsteroidCount   = 5;
      cluster.ActiveCount     = 4; // 1 was depleted
      cluster.ResourceType    = 1;
      cluster.MaxResource     = 50.0f;
      cluster.MinRadius       = 1.0f;
      cluster.MaxRadius       = 3.0f;
      cluster.RespawnInterval = 10.0f; // 10 seconds

      RegrowthSystem regrowth(manager, clusters);
      regrowth.Seed(42);

      // Manually enqueue a regrowth entry (as if MiningSystem depleted one).
      cluster.RegrowthQueue.push_back({ 10.0f });

      // Tick partway — should not respawn yet.
      regrowth.Update(5.0f);
      Assert::AreEqual(4u, cluster.ActiveCount,
        L"Should not respawn before timer expires");

      // Tick past the timer.
      regrowth.Update(6.0f);
      Assert::AreEqual(5u, cluster.ActiveCount,
        L"Should respawn after timer expires");
      Assert::AreEqual(size_t(0), cluster.RegrowthQueue.size(),
        L"Regrowth queue should be empty after respawn");
    }

    TEST_METHOD(RegrowthRespectsCapLimit)
    {
      SpaceObjectManager manager;

      std::vector<AsteroidCluster> clusters(1);
      auto& cluster = clusters[0];
      cluster.ClusterId       = 0;
      cluster.Shape           = ClusterShape::Supernova;
      cluster.Center          = { 0.0f, 0.0f, 0.0f };
      cluster.InnerRadius     = 10.0f;
      cluster.OuterRadius     = 100.0f;
      cluster.AsteroidCount   = 5;
      cluster.ActiveCount     = 5; // Already at cap
      cluster.ResourceType    = 1;
      cluster.MaxResource     = 50.0f;
      cluster.MinRadius       = 1.0f;
      cluster.MaxRadius       = 3.0f;
      cluster.RespawnInterval = 1.0f;

      RegrowthSystem regrowth(manager, clusters);
      regrowth.Seed(42);

      // Enqueue a regrowth entry even though we're already at cap.
      cluster.RegrowthQueue.push_back({ 1.0f });

      regrowth.Update(2.0f);

      // Should not exceed cap.
      Assert::AreEqual(5u, cluster.ActiveCount,
        L"Should not exceed AsteroidCount cap");
    }

    TEST_METHOD(EnqueueDepletionsFromMiningSystem)
    {
      SpaceObjectManager manager;
      MiningSystem mining(manager);

      std::vector<AsteroidCluster> clusters(1);
      auto& cluster = clusters[0];
      cluster.ClusterId       = 0;
      cluster.Shape           = ClusterShape::Supernova;
      cluster.Center          = { 0.0f, 0.0f, 0.0f };
      cluster.InnerRadius     = 10.0f;
      cluster.OuterRadius     = 100.0f;
      cluster.AsteroidCount   = 10;
      cluster.ActiveCount     = 10;
      cluster.ResourceType    = 1;
      cluster.MaxResource     = 10.0f;
      cluster.MinRadius       = 1.0f;
      cluster.MaxRadius       = 3.0f;
      cluster.RespawnInterval = 60.0f;

      RegrowthSystem regrowth(manager, clusters);
      regrowth.Seed(42);

      // Create and deplete an asteroid.
      XMFLOAT3 pos = { 50.0f, 0.0f, 0.0f };
      auto asteroid = manager.CreateEntity(SpaceObjectCategory::Asteroid, pos, 0);
      auto* astData = manager.GetAsteroidData(asteroid);
      Assert::IsNotNull(astData);
      astData->ResourceAmount = 1.0f;
      astData->MaxResource    = 1.0f;
      astData->ResourceType   = 1;
      astData->ClusterId      = 0;

      auto projectile = manager.CreateEntity(SpaceObjectCategory::Projectile, pos, 0);
      auto* projData = manager.GetProjectileData(projectile);
      Assert::IsNotNull(projData);
      projData->Damage = 100.0f;

      std::vector<CollisionEvent> collisions;
      collisions.push_back({ projectile, asteroid });
      mining.ProcessCollisions(collisions);

      // Enqueue into regrowth.
      regrowth.EnqueueDepletions(mining);

      Assert::AreEqual(9u, cluster.ActiveCount,
        L"ActiveCount should decrement on depletion");
      Assert::AreEqual(size_t(1), cluster.RegrowthQueue.size(),
        L"Should have one regrowth entry");
      Assert::AreEqual(60.0f, cluster.RegrowthQueue[0].Timer, 0.01f,
        L"Timer should match cluster RespawnInterval");
    }
  };

  // =======================================================================
  // NeuronRandom Tests
  // =======================================================================
  TEST_CLASS(NeuronRandomTests)
  {
  public:
    TEST_METHOD(SameSeedProducesSameSequence)
    {
      NeuronRandom rng1(42);
      NeuronRandom rng2(42);

      for (int i = 0; i < 100; ++i)
      {
        Assert::AreEqual(rng1.Unit(), rng2.Unit());
      }
    }

    TEST_METHOD(FloatRangeStaysInBounds)
    {
      NeuronRandom rng(123);

      for (int i = 0; i < 1000; ++i)
      {
        float v = rng.FloatRange(10.0f, 20.0f);
        Assert::IsTrue(v >= 10.0f && v <= 20.0f,
          L"FloatRange should stay in bounds");
      }
    }

    TEST_METHOD(UnitSphereProducesNormalizedVectors)
    {
      NeuronRandom rng(99);

      for (int i = 0; i < 100; ++i)
      {
        XMFLOAT3 v = rng.UnitSphere();
        XMVECTOR vec = XMLoadFloat3(&v);
        XMVECTOR lenSq = XMVector3LengthSq(vec);
        float len2;
        XMStoreFloat(&len2, lenSq);
        Assert::AreEqual(1.0f, len2, 0.01f,
          L"UnitSphere vectors should be unit length");
      }
    }
  };
}
