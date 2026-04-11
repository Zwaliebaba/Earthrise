#include "pch.h"
#include "EntityPool.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Neuron;

namespace EarthRiseTests
{
  TEST_CLASS(EntityPoolTests)
  {
  public:
    TEST_METHOD(AllocReturnsValidHandle)
    {
      GameLogic::EntityPool pool(64);
      EntityHandle handle = pool.Alloc();
      Assert::IsTrue(handle.IsValid());
      Assert::IsTrue(pool.IsValid(handle));
      Assert::AreEqual(1u, pool.ActiveCount());
    }

    TEST_METHOD(FreeInvalidatesHandle)
    {
      GameLogic::EntityPool pool(64);
      EntityHandle handle = pool.Alloc();
      pool.Free(handle);
      Assert::IsFalse(pool.IsValid(handle));
      Assert::AreEqual(0u, pool.ActiveCount());
    }

    TEST_METHOD(GenerationIncrementsOnReuse)
    {
      GameLogic::EntityPool pool(64);
      EntityHandle first = pool.Alloc();
      uint32_t firstGen = first.Generation();
      pool.Free(first);

      // Allocating again should reuse the same slot with incremented generation.
      EntityHandle second = pool.Alloc();
      Assert::AreEqual(first.Index(), second.Index());
      Assert::AreEqual(firstGen + 1, second.Generation());
    }

    TEST_METHOD(StaleHandleIsInvalid)
    {
      GameLogic::EntityPool pool(64);
      EntityHandle first = pool.Alloc();
      pool.Free(first);
      EntityHandle second = pool.Alloc();

      // The old handle should no longer be valid.
      Assert::IsFalse(pool.IsValid(first));
      Assert::IsTrue(pool.IsValid(second));
    }

    TEST_METHOD(PoolExhaustion)
    {
      GameLogic::EntityPool pool(4);
      EntityHandle h1 = pool.Alloc();
      EntityHandle h2 = pool.Alloc();
      EntityHandle h3 = pool.Alloc();
      EntityHandle h4 = pool.Alloc();

      Assert::IsTrue(h1.IsValid());
      Assert::IsTrue(h2.IsValid());
      Assert::IsTrue(h3.IsValid());
      Assert::IsTrue(h4.IsValid());
      Assert::AreEqual(4u, pool.ActiveCount());

      // Pool is full — next alloc should return invalid.
      EntityHandle h5 = pool.Alloc();
      Assert::IsFalse(h5.IsValid());

      // Free one and alloc again.
      pool.Free(h2);
      EntityHandle h6 = pool.Alloc();
      Assert::IsTrue(h6.IsValid());
      Assert::AreEqual(4u, pool.ActiveCount());
    }

    TEST_METHOD(MultipleAllocsUniqueHandles)
    {
      GameLogic::EntityPool pool(128);
      std::vector<EntityHandle> handles;

      for (uint32_t i = 0; i < 100; ++i)
      {
        EntityHandle h = pool.Alloc();
        Assert::IsTrue(h.IsValid());
        handles.push_back(h);
      }

      // All handles should be unique.
      for (size_t i = 0; i < handles.size(); ++i)
      {
        for (size_t j = i + 1; j < handles.size(); ++j)
        {
          Assert::IsFalse(handles[i] == handles[j]);
        }
      }
    }

    TEST_METHOD(FreeInvalidHandleIsNoop)
    {
      GameLogic::EntityPool pool(16);
      pool.Free(EntityHandle::Invalid());
      Assert::AreEqual(0u, pool.ActiveCount());

      // Free a handle that was never allocated.
      pool.Free(EntityHandle(999, 0));
      Assert::AreEqual(0u, pool.ActiveCount());
    }
  };
}
