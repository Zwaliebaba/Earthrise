#include "pch.h"
#include "GameTypes/EntityHandle.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Neuron;

namespace EarthRiseTests::Core
{
  TEST_CLASS(EntityHandleTests)
  {
  public:
    TEST_METHOD(DefaultHandleIsInvalid)
    {
      EntityHandle handle;
      Assert::IsFalse(handle.IsValid());
      Assert::AreEqual(0u, handle.Index());
      Assert::AreEqual(0u, handle.Generation());
    }

    TEST_METHOD(ConstructionStoresIndexAndGeneration)
    {
      EntityHandle handle(42, 7);
      Assert::IsTrue(handle.IsValid());
      Assert::AreEqual(42u, handle.Index());
      Assert::AreEqual(7u, handle.Generation());
    }

    TEST_METHOD(MaxIndexAndGeneration)
    {
      EntityHandle handle(EntityHandle::MAX_INDEX, EntityHandle::MAX_GENERATION);
      Assert::AreEqual(EntityHandle::MAX_INDEX, handle.Index());
      Assert::AreEqual(EntityHandle::MAX_GENERATION, handle.Generation());
    }

    TEST_METHOD(InvalidStaticFactory)
    {
      EntityHandle handle = EntityHandle::Invalid();
      Assert::IsFalse(handle.IsValid());
    }

    TEST_METHOD(EqualityComparison)
    {
      EntityHandle a(10, 3);
      EntityHandle b(10, 3);
      EntityHandle c(10, 4);
      EntityHandle d(11, 3);

      Assert::IsTrue(a == b);
      Assert::IsFalse(a == c);
      Assert::IsFalse(a == d);
    }

    TEST_METHOD(DifferentGenerationsMakeHandlesUnequal)
    {
      // Same index, different generation — should not be equal.
      // This is the core of stale-handle detection.
      EntityHandle gen0(5, 0);
      EntityHandle gen1(5, 1);
      Assert::IsFalse(gen0 == gen1);
    }
  };
}
