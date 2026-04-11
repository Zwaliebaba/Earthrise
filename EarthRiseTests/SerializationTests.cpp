#include "pch.h"
#include "GameTypes/SpaceObject.h"
#include "GameTypes/ObjectDefs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Neuron;

namespace EarthRiseTests
{
  TEST_CLASS(SerializationTests)
  {
  public:
    TEST_METHOD(SpaceObjectRoundTrip)
    {
      // Create a SpaceObject with known values.
      SpaceObject original;
      original.Handle       = EntityHandle(42, 7);
      original.Category     = SpaceObjectCategory::Ship;
      original.Active       = true;
      original.Position     = { 100.0f, 200.0f, 300.0f };
      original.Orientation  = { 0.0f, 0.707f, 0.0f, 0.707f };
      original.Velocity     = { 10.0f, 0.0f, -5.0f };
      original.BoundingRadius = 25.0f;
      original.Color        = { 0.2f, 0.8f, 0.4f, 1.0f };
      original.MeshNameHash = 0xDEADBEEF;
      original.DefIndex     = 99;

      // Serialize via DataWriter.
      DataWriter writer;
      writer.Write(original.Handle.m_id);
      writer.Write(original.Category);
      writer.Write(original.Active);
      writer.Write(original.Position);
      writer.Write(original.Orientation);
      writer.Write(original.Velocity);
      writer.Write(original.BoundingRadius);
      writer.Write(original.Color);
      writer.Write(original.MeshNameHash);
      writer.Write(original.DefIndex);

      // Deserialize via DataReader.
      DataReader reader(reinterpret_cast<const uint8_t*>(writer.Data()), static_cast<size_t>(writer.Size()));

      SpaceObject deserialized;
      deserialized.Handle       = EntityHandle(reader.Read<uint32_t>(), 0);
      // Reconstruct properly:
      uint32_t rawId = deserialized.Handle.m_id; // Actually we serialized m_id directly
      (void)rawId;

      // Re-do: read back the raw m_id
      DataReader reader2(reinterpret_cast<const uint8_t*>(writer.Data()), static_cast<size_t>(writer.Size()));
      SpaceObject result;
      result.Handle.m_id     = reader2.Read<uint32_t>();
      result.Category        = reader2.Read<SpaceObjectCategory>();
      result.Active          = reader2.Read<bool>();
      result.Position        = reader2.Read<XMFLOAT3>();
      result.Orientation     = reader2.Read<XMFLOAT4>();
      result.Velocity        = reader2.Read<XMFLOAT3>();
      result.BoundingRadius  = reader2.Read<float>();
      result.Color           = reader2.Read<XMFLOAT4>();
      result.MeshNameHash    = reader2.Read<uint32_t>();
      result.DefIndex        = reader2.Read<uint16_t>();

      // Verify round-trip.
      Assert::IsTrue(result.Handle == original.Handle);
      Assert::IsTrue(result.Category == original.Category);
      Assert::AreEqual(original.Active, result.Active);
      Assert::AreEqual(original.Position.x, result.Position.x);
      Assert::AreEqual(original.Position.y, result.Position.y);
      Assert::AreEqual(original.Position.z, result.Position.z);
      Assert::AreEqual(original.Orientation.x, result.Orientation.x);
      Assert::AreEqual(original.Orientation.y, result.Orientation.y);
      Assert::AreEqual(original.Orientation.z, result.Orientation.z);
      Assert::AreEqual(original.Orientation.w, result.Orientation.w);
      Assert::AreEqual(original.Velocity.x, result.Velocity.x);
      Assert::AreEqual(original.Velocity.y, result.Velocity.y);
      Assert::AreEqual(original.Velocity.z, result.Velocity.z);
      Assert::AreEqual(original.BoundingRadius, result.BoundingRadius);
      Assert::AreEqual(original.Color.x, result.Color.x);
      Assert::AreEqual(original.Color.y, result.Color.y);
      Assert::AreEqual(original.Color.z, result.Color.z);
      Assert::AreEqual(original.Color.w, result.Color.w);
      Assert::AreEqual(original.MeshNameHash, result.MeshNameHash);
      Assert::AreEqual(original.DefIndex, result.DefIndex);
    }

    TEST_METHOD(BinaryDataWriterReaderRoundTrip)
    {
      BinaryDataWriter writer;
      writer.Write<uint32_t>(123);
      writer.Write<float>(3.14f);
      writer.WriteString("Hello World");
      writer.Write<uint16_t>(9999);

      BinaryDataReader reader(writer.Buffer());
      Assert::AreEqual(123u, reader.Read<uint32_t>());
      Assert::AreEqual(3.14f, reader.Read<float>());
      Assert::AreEqual(std::string("Hello World"), reader.ReadString());
      Assert::AreEqual(static_cast<uint16_t>(9999), reader.Read<uint16_t>());
      Assert::IsTrue(reader.AtEnd());
    }

    TEST_METHOD(HashMeshNameConsistency)
    {
      uint32_t h1 = Neuron::HashMeshName("HullAvalanche");
      uint32_t h2 = Neuron::HashMeshName("HullAvalanche");
      uint32_t h3 = Neuron::HashMeshName("Asteroid01");

      Assert::AreEqual(h1, h2);
      Assert::AreNotEqual(h1, h3);
      Assert::AreNotEqual(h1, 0u);
    }
  };
}
