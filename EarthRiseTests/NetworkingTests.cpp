#include "pch.h"
#include "BandwidthManager.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Neuron;

namespace EarthRiseTests::Networking
{
  // ── Packet framing tests ──────────────────────────────────
  TEST_CLASS(PacketFramingTests)
  {
  public:
    TEST_METHOD(FrameAndParseRoundTrip)
    {
      // Write a payload, frame it, parse it back.
      const char payload[] = "Hello, server!";
      int payloadSize = static_cast<int>(strlen(payload));

      std::array<uint8_t, DATALOAD_SIZE> buffer{};
      int framed = FramePacket(MessageId::ChatMessage, 42, PACKET_FLAG_RELIABLE,
                               payload, payloadSize,
                               buffer.data(), static_cast<int>(buffer.size()));

      Assert::IsTrue(framed > 0, L"FramePacket should succeed");
      Assert::AreEqual(static_cast<int>(sizeof(PacketHeader)) + payloadSize, framed);

      PacketHeader header{};
      const uint8_t* parsedPayload = nullptr;
      int parsedSize = 0;

      bool ok = ParsePacket(buffer.data(), framed, header, parsedPayload, parsedSize);
      Assert::IsTrue(ok, L"ParsePacket should succeed");
      Assert::AreEqual(static_cast<uint16_t>(MessageId::ChatMessage), header.MessageId);
      Assert::AreEqual(static_cast<uint16_t>(42), header.SequenceNumber);
      Assert::AreEqual(PACKET_FLAG_RELIABLE, header.Flags);
      Assert::AreEqual(payloadSize, parsedSize);
      Assert::AreEqual(0, memcmp(payload, parsedPayload, payloadSize));
    }

    TEST_METHOD(FrameEmptyPayload)
    {
      std::array<uint8_t, DATALOAD_SIZE> buffer{};
      int framed = FramePacket(MessageId::Heartbeat, 1, 0,
                               nullptr, 0,
                               buffer.data(), static_cast<int>(buffer.size()));

      Assert::AreEqual(static_cast<int>(sizeof(PacketHeader)), framed);

      PacketHeader header{};
      const uint8_t* payload = nullptr;
      int payloadSize = 0;

      bool ok = ParsePacket(buffer.data(), framed, header, payload, payloadSize);
      Assert::IsTrue(ok);
      Assert::AreEqual(0, payloadSize);
    }

    TEST_METHOD(ParseRejectsTruncatedDatagram)
    {
      PacketHeader header{};
      const uint8_t* payload = nullptr;
      int payloadSize = 0;

      // Too small to contain a header.
      uint8_t tiny[4] = {};
      Assert::IsFalse(ParsePacket(tiny, sizeof(tiny), header, payload, payloadSize));
    }

    TEST_METHOD(ParseRejectsOversizedPayload)
    {
      // Forge a header claiming more payload than the buffer contains.
      PacketHeader fakeHeader{};
      fakeHeader.MessageId = static_cast<uint16_t>(MessageId::Heartbeat);
      fakeHeader.PayloadSize = 9999; // way beyond buffer

      std::array<uint8_t, 16> buffer{};
      memcpy(buffer.data(), &fakeHeader, sizeof(fakeHeader));

      PacketHeader parsed{};
      const uint8_t* payload = nullptr;
      int payloadSize = 0;

      Assert::IsFalse(ParsePacket(buffer.data(), static_cast<int>(buffer.size()),
                                  parsed, payload, payloadSize));
    }

    TEST_METHOD(FrameRejectsOversizedPayload)
    {
      std::array<uint8_t, DATALOAD_SIZE> buffer{};
      // Payload larger than MAX_PAYLOAD_SIZE should fail.
      int result = FramePacket(MessageId::Heartbeat, 0, 0,
                               buffer.data(), MAX_PAYLOAD_SIZE + 1,
                               buffer.data(), static_cast<int>(buffer.size()));
      Assert::AreEqual(-1, result);
    }

    TEST_METHOD(PacketHeaderIsTightlyPacked)
    {
      Assert::AreEqual(static_cast<size_t>(8), sizeof(PacketHeader));
    }
  };

  // ── Message serialization tests ───────────────────────────
  TEST_CLASS(MessageSerializationTests)
  {
  public:
    TEST_METHOD(LoginRequestRoundTrip)
    {
      LoginRequest original;
      original.PlayerName = "TestPlayer";
      original.ProtocolVersion = ENGINE_VERSION;

      DataWriter w;
      original.Write(w);

      DataReader r(reinterpret_cast<const uint8_t*>(w.Data()), w.Size());
      LoginRequest parsed;
      parsed.Read(r);

      Assert::AreEqual(original.PlayerName, parsed.PlayerName);
      Assert::AreEqual(original.ProtocolVersion, parsed.ProtocolVersion);
    }

    TEST_METHOD(LoginResponseRoundTrip)
    {
      LoginResponse original;
      original.Result = LoginResult::VersionMismatch;
      original.SessionId = 12345;

      DataWriter w;
      original.Write(w);

      DataReader r(reinterpret_cast<const uint8_t*>(w.Data()), w.Size());
      LoginResponse parsed;
      parsed.Read(r);

      Assert::IsTrue(original.Result == parsed.Result);
      Assert::AreEqual(original.SessionId, parsed.SessionId);
    }

    TEST_METHOD(DisconnectMsgRoundTrip)
    {
      DisconnectMsg original;
      original.Reason = DisconnectReason::Kicked;

      DataWriter w;
      original.Write(w);

      DataReader r(reinterpret_cast<const uint8_t*>(w.Data()), w.Size());
      DisconnectMsg parsed;
      parsed.Read(r);

      Assert::IsTrue(original.Reason == parsed.Reason);
    }

    TEST_METHOD(HeartbeatMsgRoundTrip)
    {
      HeartbeatMsg original;
      original.ClientTime = 987654;

      DataWriter w;
      original.Write(w);

      DataReader r(reinterpret_cast<const uint8_t*>(w.Data()), w.Size());
      HeartbeatMsg parsed;
      parsed.Read(r);

      Assert::AreEqual(original.ClientTime, parsed.ClientTime);
    }

    TEST_METHOD(EntitySpawnMsgRoundTrip)
    {
      EntitySpawnMsg original;
      original.Handle = EntityHandle(42, 7);
      original.Category = SpaceObjectCategory::Ship;
      original.MeshHash = 0xDEADBEEF;
      original.Position = { 100.0f, 200.0f, 300.0f };
      original.Orientation = { 0.0f, 0.707f, 0.0f, 0.707f };
      original.Color = { 0.5f, 1.0f, 0.0f, 1.0f };
      original.Surface = SurfaceType::Desert;

      DataWriter w;
      original.Write(w);

      DataReader r(reinterpret_cast<const uint8_t*>(w.Data()), w.Size());
      EntitySpawnMsg parsed;
      parsed.Read(r);

      Assert::AreEqual(original.Handle.m_id, parsed.Handle.m_id);
      Assert::IsTrue(original.Category == parsed.Category);
      Assert::AreEqual(original.MeshHash, parsed.MeshHash);
      Assert::AreEqual(original.Position.x, parsed.Position.x);
      Assert::AreEqual(original.Position.y, parsed.Position.y);
      Assert::AreEqual(original.Position.z, parsed.Position.z);
      Assert::AreEqual(original.Orientation.w, parsed.Orientation.w);
      Assert::AreEqual(original.Color.x, parsed.Color.x);
      Assert::IsTrue(original.Surface == parsed.Surface);
    }

    TEST_METHOD(EntityDespawnMsgRoundTrip)
    {
      EntityDespawnMsg original;
      original.Handle = EntityHandle(99, 3);

      DataWriter w;
      original.Write(w);

      DataReader r(reinterpret_cast<const uint8_t*>(w.Data()), w.Size());
      EntityDespawnMsg parsed;
      parsed.Read(r);

      Assert::AreEqual(original.Handle.m_id, parsed.Handle.m_id);
    }

    TEST_METHOD(StateSnapshotMsgRoundTrip)
    {
      StateSnapshotMsg original;
      original.ServerTick = 12345;
      original.Entities.push_back({ 1, { 10, 20, 30 }, { 0, 0, 0, 1 }, { 1, 0, 0 } });
      original.Entities.push_back({ 2, { 40, 50, 60 }, { 0, 0.707f, 0, 0.707f }, { 0, 1, 0 } });

      DataWriter w;
      original.Write(w);

      DataReader r(reinterpret_cast<const uint8_t*>(w.Data()), w.Size());
      StateSnapshotMsg parsed;
      parsed.Read(r);

      Assert::AreEqual(original.ServerTick, parsed.ServerTick);
      Assert::AreEqual(static_cast<size_t>(2), parsed.Entities.size());
      Assert::AreEqual(original.Entities[0].HandleId, parsed.Entities[0].HandleId);
      Assert::AreEqual(original.Entities[1].Position.y, parsed.Entities[1].Position.y);
      Assert::AreEqual(original.Entities[0].Velocity.x, parsed.Entities[0].Velocity.x);
    }

    TEST_METHOD(PlayerCommandMsgRoundTrip)
    {
      PlayerCommandMsg original;
      original.Type = CommandType::AttackTarget;
      original.TargetHandle = EntityHandle(55, 2);
      original.TargetPosition = { 500.0f, 0.0f, -300.0f };

      DataWriter w;
      original.Write(w);

      DataReader r(reinterpret_cast<const uint8_t*>(w.Data()), w.Size());
      PlayerCommandMsg parsed;
      parsed.Read(r);

      Assert::IsTrue(original.Type == parsed.Type);
      Assert::AreEqual(original.TargetHandle.m_id, parsed.TargetHandle.m_id);
      Assert::AreEqual(original.TargetPosition.z, parsed.TargetPosition.z);
    }

    TEST_METHOD(ChatMsgRoundTrip)
    {
      ChatMsg original;
      original.Channel = ChatChannel::Party;
      original.SenderName = "Alice";
      original.Text = "Hello, world!";

      DataWriter w;
      original.Write(w);

      DataReader r(reinterpret_cast<const uint8_t*>(w.Data()), w.Size());
      ChatMsg parsed;
      parsed.Read(r);

      Assert::IsTrue(original.Channel == parsed.Channel);
      Assert::AreEqual(original.SenderName, parsed.SenderName);
      Assert::AreEqual(original.Text, parsed.Text);
    }
  };

  // ── Reliable channel tests ────────────────────────────────
  TEST_CLASS(ReliableChannelTests)
  {
  public:
    TEST_METHOD(QueueAndAcknowledge)
    {
      ReliableChannel channel;
      uint8_t data[] = { 1, 2, 3 };

      uint16_t seq = channel.QueueReliable(data, sizeof(data), 0.0);
      Assert::AreEqual(static_cast<uint16_t>(1), seq);

      // Before ACK, resend should return the packet after timeout.
      std::vector<PendingPacket> resends;
      channel.CollectResends(1.0, resends); // 1 second later
      Assert::AreEqual(static_cast<size_t>(1), resends.size());

      // After ACK, no resends.
      channel.Acknowledge(seq);
      resends.clear();
      channel.CollectResends(2.0, resends);
      Assert::AreEqual(static_cast<size_t>(0), resends.size());
    }

    TEST_METHOD(DuplicateDetection)
    {
      ReliableChannel channel;

      Assert::IsFalse(channel.IsDuplicate(1));
      channel.MarkReceived(1);
      Assert::IsTrue(channel.IsDuplicate(1));
      Assert::IsFalse(channel.IsDuplicate(2));
    }

    TEST_METHOD(SequenceIncrementsPerSend)
    {
      ReliableChannel channel;
      uint8_t data[] = { 0 };

      uint16_t s1 = channel.QueueReliable(data, 1, 0.0);
      uint16_t s2 = channel.QueueReliable(data, 1, 0.0);
      uint16_t s3 = channel.QueueReliable(data, 1, 0.0);

      Assert::AreEqual(static_cast<uint16_t>(1), s1);
      Assert::AreEqual(static_cast<uint16_t>(2), s2);
      Assert::AreEqual(static_cast<uint16_t>(3), s3);
    }

    TEST_METHOD(NoResendBeforeTimeout)
    {
      ReliableChannel channel;
      uint8_t data[] = { 0 };
      channel.QueueReliable(data, 1, 1.0);

      std::vector<PendingPacket> resends;
      // Only 0.1 seconds later — should NOT resend.
      channel.CollectResends(1.1, resends);
      Assert::AreEqual(static_cast<size_t>(0), resends.size());
    }

    TEST_METHOD(BuildAckFramesCorrectly)
    {
      std::array<uint8_t, DATALOAD_SIZE> buf{};
      int size = ReliableChannel::BuildAck(42, buf.data(), static_cast<int>(buf.size()));
      Assert::IsTrue(size > 0);

      PacketHeader header{};
      const uint8_t* payload = nullptr;
      int payloadSize = 0;

      bool ok = ParsePacket(buf.data(), size, header, payload, payloadSize);
      Assert::IsTrue(ok);
      Assert::AreEqual(static_cast<uint16_t>(MessageId::Ack), header.MessageId);
      Assert::AreEqual(static_cast<uint16_t>(42), header.SequenceNumber);
      Assert::AreEqual(0, payloadSize);
    }
  };

  // ── Bandwidth manager tests ───────────────────────────────
  TEST_CLASS(BandwidthManagerTests)
  {
  public:
    TEST_METHOD(BudgetEnforcedPerSecond)
    {
      Neuron::Server::BandwidthManager bw(1000, 10000.0f); // 1000 bytes/s

      Assert::IsTrue(bw.CanSend(1, 500, 0.0));
      bw.RecordSend(1, 500, 0.0);
      Assert::IsTrue(bw.CanSend(1, 500, 0.0));
      bw.RecordSend(1, 500, 0.0);
      Assert::IsFalse(bw.CanSend(1, 1, 0.0)); // Budget exhausted

      // New window resets.
      Assert::IsTrue(bw.CanSend(1, 500, 1.1));
    }

    TEST_METHOD(AreaOfInterestFiltering)
    {
      Neuron::Server::BandwidthManager bw(64 * 1024, 100.0f); // 100 m AoI

      XMVECTOR cam = XMVectorSet(0, 0, 0, 0);
      XMVECTOR nearPos = XMVectorSet(50, 0, 0, 0);
      XMVECTOR farPos = XMVectorSet(200, 0, 0, 0);

      Assert::IsTrue(bw.IsInAreaOfInterest(cam, nearPos));
      Assert::IsFalse(bw.IsInAreaOfInterest(cam, farPos));
    }

    TEST_METHOD(SnapshotRateLimiting)
    {
      Neuron::Server::BandwidthManager bw;

      Assert::IsTrue(bw.CanSendSnapshot(1, 0.0));
      bw.RecordSnapshot(1, 0.0);

      // Too soon — should be throttled.
      Assert::IsFalse(bw.CanSendSnapshot(1, 0.01));

      // After min interval — should be allowed.
      Assert::IsTrue(bw.CanSendSnapshot(1, 0.06));
    }

    TEST_METHOD(RemoveClientClearsBudget)
    {
      Neuron::Server::BandwidthManager bw(100, 10000.0f);

      bw.RecordSend(1, 100, 0.0);
      Assert::IsFalse(bw.CanSend(1, 1, 0.0));

      bw.RemoveClient(1);
      Assert::IsTrue(bw.CanSend(1, 50, 0.0)); // Fresh budget
    }
  };

  // ── Full message frame round-trip (write → frame → parse → read) ──
  TEST_CLASS(FullFrameRoundTripTests)
  {
  public:
    TEST_METHOD(ChatMessageFullRoundTrip)
    {
      // 1. Serialize the message payload.
      ChatMsg original;
      original.Channel = ChatChannel::Whisper;
      original.SenderName = "Bob";
      original.Text = "Hey there!";

      DataWriter w;
      original.Write(w);

      // 2. Frame into a datagram.
      std::array<uint8_t, DATALOAD_SIZE> datagram{};
      int framed = FramePacket(MessageId::ChatMessage, 7, PACKET_FLAG_RELIABLE,
                               w.Data(), w.Size(),
                               datagram.data(), static_cast<int>(datagram.size()));
      Assert::IsTrue(framed > 0);

      // 3. Parse the datagram header.
      PacketHeader header{};
      const uint8_t* payload = nullptr;
      int payloadSize = 0;
      bool ok = ParsePacket(datagram.data(), framed, header, payload, payloadSize);
      Assert::IsTrue(ok);

      // 4. Deserialize the payload.
      DataReader r(payload, payloadSize);
      ChatMsg parsed;
      parsed.Read(r);

      Assert::IsTrue(parsed.Channel == ChatChannel::Whisper);
      Assert::AreEqual(std::string("Bob"), parsed.SenderName);
      Assert::AreEqual(std::string("Hey there!"), parsed.Text);
    }

    TEST_METHOD(EntitySpawnFullRoundTrip)
    {
      EntitySpawnMsg original;
      original.Handle = EntityHandle(100, 5);
      original.Category = SpaceObjectCategory::Asteroid;
      original.MeshHash = 0xCAFEBABE;
      original.Position = { 1000, 2000, 3000 };
      original.Orientation = { 0, 0, 0, 1 };
      original.Color = { 1, 0.5f, 0.25f, 1 };
      original.Surface = SurfaceType::IceCap;

      DataWriter w;
      original.Write(w);

      std::array<uint8_t, DATALOAD_SIZE> datagram{};
      int framed = FramePacket(MessageId::EntitySpawn, 1, PACKET_FLAG_RELIABLE,
                               w.Data(), w.Size(),
                               datagram.data(), static_cast<int>(datagram.size()));
      Assert::IsTrue(framed > 0);

      PacketHeader header{};
      const uint8_t* payload = nullptr;
      int payloadSize = 0;
      Assert::IsTrue(ParsePacket(datagram.data(), framed, header, payload, payloadSize));

      DataReader r(payload, payloadSize);
      EntitySpawnMsg parsed;
      parsed.Read(r);

      Assert::AreEqual(original.Handle.m_id, parsed.Handle.m_id);
      Assert::AreEqual(original.Position.x, parsed.Position.x);
      Assert::AreEqual(original.Color.y, parsed.Color.y);
      Assert::IsTrue(original.Surface == parsed.Surface);
    }
  };
}
