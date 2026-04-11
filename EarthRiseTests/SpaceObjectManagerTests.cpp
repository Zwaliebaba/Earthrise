#include "pch.h"
#include "SpaceObjectManager.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Neuron;

namespace EarthRiseTests
{
  TEST_CLASS(SpaceObjectManagerTests)
  {
  public:
    TEST_METHOD(CreateEntityReturnsValidHandle)
    {
      GameLogic::SpaceObjectManager mgr;
      EntityHandle handle = mgr.CreateEntity(SpaceObjectCategory::Ship);
      Assert::IsTrue(handle.IsValid());
      Assert::IsTrue(mgr.IsValid(handle));
      Assert::AreEqual(1u, mgr.ActiveCount());
    }

    TEST_METHOD(SpaceObjectHasCorrectCategory)
    {
      GameLogic::SpaceObjectManager mgr;
      EntityHandle handle = mgr.CreateEntity(SpaceObjectCategory::Asteroid,
        XMFLOAT3{ 100.0f, 200.0f, 300.0f }, 0xDEAD, 5);

      SpaceObject* obj = mgr.GetSpaceObject(handle);
      Assert::IsNotNull(obj);
      Assert::IsTrue(obj->Category == SpaceObjectCategory::Asteroid);
      Assert::IsTrue(obj->Active);
      Assert::AreEqual(100.0f, obj->Position.x);
      Assert::AreEqual(200.0f, obj->Position.y);
      Assert::AreEqual(300.0f, obj->Position.z);
      Assert::AreEqual(0xDEADu, obj->MeshNameHash);
      Assert::AreEqual(static_cast<uint16_t>(5), obj->DefIndex);
    }

    TEST_METHOD(DestroyEntityInvalidatesHandle)
    {
      GameLogic::SpaceObjectManager mgr;
      EntityHandle handle = mgr.CreateEntity(SpaceObjectCategory::Crate);
      mgr.DestroyEntity(handle);
      Assert::IsFalse(mgr.IsValid(handle));
      Assert::IsNull(mgr.GetSpaceObject(handle));
      Assert::AreEqual(0u, mgr.ActiveCount());
    }

    TEST_METHOD(CategoryDataAccessCorrectType)
    {
      GameLogic::SpaceObjectManager mgr;
      EntityHandle ship = mgr.CreateEntity(SpaceObjectCategory::Ship);
      EntityHandle asteroid = mgr.CreateEntity(SpaceObjectCategory::Asteroid);

      // Ship should have ShipData, not AsteroidData.
      Assert::IsNotNull(mgr.GetShipData(ship));
      Assert::IsNull(mgr.GetAsteroidData(ship));

      // Asteroid should have AsteroidData, not ShipData.
      Assert::IsNotNull(mgr.GetAsteroidData(asteroid));
      Assert::IsNull(mgr.GetShipData(asteroid));
    }

    TEST_METHOD(CreateAllCategories)
    {
      GameLogic::SpaceObjectManager mgr;

      EntityHandle handles[static_cast<int>(SpaceObjectCategory::COUNT)];
      for (auto cat : RangeSpaceObjectCategory())
      {
        auto h = mgr.CreateEntity(cat);
        handles[static_cast<int>(cat)] = h;
        Assert::IsTrue(h.IsValid());
      }

      Assert::AreEqual(static_cast<uint32_t>(SizeOfSpaceObjectCategory()), mgr.ActiveCount());

      // Verify each has correct category.
      for (auto cat : RangeSpaceObjectCategory())
      {
        SpaceObject* obj = mgr.GetSpaceObject(handles[static_cast<int>(cat)]);
        Assert::IsNotNull(obj);
        Assert::IsTrue(obj->Category == cat);
      }

      // Destroy all.
      for (auto cat : RangeSpaceObjectCategory())
      {
        mgr.DestroyEntity(handles[static_cast<int>(cat)]);
      }
      Assert::AreEqual(0u, mgr.ActiveCount());
    }

    TEST_METHOD(ForEachActiveVisitsAllActive)
    {
      GameLogic::SpaceObjectManager mgr;
      [[maybe_unused]] auto h1 = mgr.CreateEntity(SpaceObjectCategory::Ship);
      [[maybe_unused]] auto h2 = mgr.CreateEntity(SpaceObjectCategory::Asteroid);
      [[maybe_unused]] auto h3 = mgr.CreateEntity(SpaceObjectCategory::Station);

      uint32_t count = 0;
      mgr.ForEachActive([&count](SpaceObject&) { ++count; });
      Assert::AreEqual(3u, count);
    }

    TEST_METHOD(ForEachCategoryFilters)
    {
      GameLogic::SpaceObjectManager mgr;
      [[maybe_unused]] auto h1 = mgr.CreateEntity(SpaceObjectCategory::Ship);
      [[maybe_unused]] auto h2 = mgr.CreateEntity(SpaceObjectCategory::Ship);
      [[maybe_unused]] auto h3 = mgr.CreateEntity(SpaceObjectCategory::Asteroid);

      uint32_t shipCount = 0;
      mgr.ForEachCategory(SpaceObjectCategory::Ship, [&shipCount](SpaceObject&) { ++shipCount; });
      Assert::AreEqual(2u, shipCount);

      uint32_t asteroidCount = 0;
      mgr.ForEachCategory(SpaceObjectCategory::Asteroid, [&asteroidCount](SpaceObject&) { ++asteroidCount; });
      Assert::AreEqual(1u, asteroidCount);
    }

    TEST_METHOD(GetSpaceObjectInvalidHandleReturnsNull)
    {
      GameLogic::SpaceObjectManager mgr;
      Assert::IsNull(mgr.GetSpaceObject(EntityHandle::Invalid()));
    }
  };
}
