#include "pch.h"

// UITests.cpp — Phase 8 unit tests for HUD, Targeting, Chat, Fleet Panel, and Jumpgate UI.
// Tests focus on data binding and computation logic (no GPU required).

#include "HUD.h"
#include "TargetingUI.h"
#include "ChatUI.h"
#include "FleetPanel.h"
#include "JumpgateUI.h"
#include "ClientWorldState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Neuron;

namespace
{
  // Helper: spawn a test entity into the client world state.
  void SpawnTestEntity(ClientWorldState& _world, uint32_t _id,
    SpaceObjectCategory _cat, const XMFLOAT3& _pos,
    float _hullHP = 0.0f, float _hullMaxHP = 0.0f,
    float _shieldHP = 0.0f, float _shieldMaxHP = 0.0f)
  {
    EntitySpawnMsg msg;
    msg.Handle.m_id = _id;
    msg.Category    = _cat;
    msg.MeshHash    = _id;
    msg.Position    = _pos;
    msg.Orientation = { 0, 0, 0, 1 };
    msg.Color       = { 1, 1, 1, 1 };
    _world.ApplySpawn(msg);

    // Apply ship stats if provided
    if (_hullMaxHP > 0.0f)
    {
      ShipStatusMsg status;
      ShipStatusEntry entry;
      entry.HandleId    = _id;
      entry.HullHP      = _hullHP;
      entry.HullMaxHP   = _hullMaxHP;
      entry.ShieldHP    = _shieldHP;
      entry.ShieldMaxHP = _shieldMaxHP;
      entry.ArmorHP     = 50.0f;
      entry.ArmorMaxHP  = 100.0f;
      entry.Energy      = 75.0f;
      entry.EnergyMax   = 100.0f;
      entry.Speed       = 10.0f;
      entry.MaxSpeed    = 100.0f;
      entry.FactionId   = 1;
      status.Ships.push_back(entry);
      _world.ApplyShipStatus(status);
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// HUD Layout Tests
// ═══════════════════════════════════════════════════════════════════════════
TEST_CLASS(HUDLayoutTests)
{
public:
  TEST_METHOD(ComputeBar_HalfFull)
  {
    auto bar = HUDLayout::ComputeBar(50.0f, 100.0f, 10, 20, 200, 12);
    Assert::AreEqual(0.5f, bar.FillFraction, 0.001f);
    Assert::AreEqual(10L, bar.Fill.left);
    Assert::AreEqual(110L, bar.Fill.right); // 10 + 200*0.5
    Assert::AreEqual(20L, bar.Fill.top);
    Assert::AreEqual(32L, bar.Fill.bottom); // 20 + 12
  }

  TEST_METHOD(ComputeBar_Full)
  {
    auto bar = HUDLayout::ComputeBar(100.0f, 100.0f, 0, 0, 200, 10);
    Assert::AreEqual(1.0f, bar.FillFraction, 0.001f);
    Assert::AreEqual(200L, bar.Fill.right);
  }

  TEST_METHOD(ComputeBar_Empty)
  {
    auto bar = HUDLayout::ComputeBar(0.0f, 100.0f, 0, 0, 200, 10);
    Assert::AreEqual(0.0f, bar.FillFraction, 0.001f);
    Assert::AreEqual(0L, bar.Fill.right);
  }

  TEST_METHOD(ComputeBar_ZeroMax)
  {
    auto bar = HUDLayout::ComputeBar(50.0f, 0.0f, 0, 0, 200, 10);
    Assert::AreEqual(0.0f, bar.FillFraction, 0.001f);
  }

  TEST_METHOD(ComputeBar_OverMax_ClampsToOne)
  {
    auto bar = HUDLayout::ComputeBar(150.0f, 100.0f, 0, 0, 200, 10);
    Assert::AreEqual(1.0f, bar.FillFraction, 0.001f);
    Assert::AreEqual(200L, bar.Fill.right);
  }

  TEST_METHOD(ComputeBar_Negative_ClampsToZero)
  {
    auto bar = HUDLayout::ComputeBar(-10.0f, 100.0f, 0, 0, 200, 10);
    Assert::AreEqual(0.0f, bar.FillFraction, 0.001f);
  }

  TEST_METHOD(ShieldBar_PositionedCorrectly)
  {
    auto bar = HUDLayout::ShieldBar(50.0f, 100.0f, 1920, 1080);
    Assert::AreEqual(HUDLayout::MARGIN, bar.Outline.left);
    Assert::AreEqual(HUDLayout::MARGIN + HUDLayout::BAR_WIDTH, bar.Outline.right);
    Assert::AreEqual(0.5f, bar.FillFraction, 0.001f);
  }

  TEST_METHOD(TargetBars_PositionedTopRight)
  {
    auto bar = HUDLayout::TargetShieldBar(80.0f, 100.0f, 1920, 1080);
    Assert::AreEqual(static_cast<LONG>(1920) - HUDLayout::MARGIN - HUDLayout::TARGET_BAR_WIDTH,
      bar.Outline.left);
    Assert::AreEqual(HUDLayout::MARGIN, bar.Outline.top);
    Assert::AreEqual(0.8f, bar.FillFraction, 0.001f);
  }

  TEST_METHOD(AllPlayerBars_DifferentVerticalPositions)
  {
    auto shield = HUDLayout::ShieldBar(100, 100, 1920, 1080);
    auto armor  = HUDLayout::ArmorBar(100, 100, 1920, 1080);
    auto hull   = HUDLayout::HullBar(100, 100, 1920, 1080);
    auto energy = HUDLayout::EnergyBar(100, 100, 1920, 1080);
    auto speed  = HUDLayout::SpeedBar(100, 100, 1920, 1080);

    // All should have the same left position
    Assert::AreEqual(shield.Outline.left, armor.Outline.left);
    Assert::AreEqual(shield.Outline.left, hull.Outline.left);
    Assert::AreEqual(shield.Outline.left, energy.Outline.left);
    Assert::AreEqual(shield.Outline.left, speed.Outline.left);

    // All should be at different vertical positions
    Assert::AreNotEqual(shield.Outline.top, armor.Outline.top);
    Assert::AreNotEqual(armor.Outline.top, hull.Outline.top);
    Assert::AreNotEqual(hull.Outline.top, energy.Outline.top);
    Assert::AreNotEqual(energy.Outline.top, speed.Outline.top);
  }
};

// ═══════════════════════════════════════════════════════════════════════════
// Targeting UI Tests
// ═══════════════════════════════════════════════════════════════════════════
TEST_CLASS(TargetingUITests)
{
public:
  TEST_METHOD(RefreshTargetList_FindsShips)
  {
    ClientWorldState world;
    SpawnTestEntity(world, 1, SpaceObjectCategory::Ship, { 0, 0, 0 });
    SpawnTestEntity(world, 2, SpaceObjectCategory::Ship, { 10, 0, 0 });
    SpawnTestEntity(world, 3, SpaceObjectCategory::Asteroid, { 20, 0, 0 });

    TargetingUI targeting;
    targeting.Initialize(&world, nullptr);
    targeting.RefreshTargetList();

    Assert::AreEqual(size_t(2), targeting.TargetCount());
  }

  TEST_METHOD(RefreshTargetList_RespectsFilter)
  {
    ClientWorldState world;
    SpawnTestEntity(world, 1, SpaceObjectCategory::Ship, { 0, 0, 0 });
    SpawnTestEntity(world, 2, SpaceObjectCategory::Station, { 10, 0, 0 });

    TargetingUI targeting;
    targeting.Initialize(&world, nullptr);
    targeting.TargetStations = false;
    targeting.RefreshTargetList();

    Assert::AreEqual(size_t(1), targeting.TargetCount());
  }

  TEST_METHOD(CycleNext_CyclesThroughAll)
  {
    ClientWorldState world;
    SpawnTestEntity(world, 10, SpaceObjectCategory::Ship, { 0, 0, 0 });
    SpawnTestEntity(world, 20, SpaceObjectCategory::Ship, { 10, 0, 0 });
    SpawnTestEntity(world, 30, SpaceObjectCategory::Ship, { 20, 0, 0 });

    TargetingUI targeting;
    targeting.Initialize(&world, nullptr);
    targeting.RefreshTargetList();

    auto first = targeting.CycleNext();
    Assert::AreEqual(10u, first.m_id);

    auto second = targeting.CycleNext();
    Assert::AreEqual(20u, second.m_id);

    auto third = targeting.CycleNext();
    Assert::AreEqual(30u, third.m_id);

    // Wraps around
    auto wrap = targeting.CycleNext();
    Assert::AreEqual(10u, wrap.m_id);
  }

  TEST_METHOD(CyclePrev_Reverses)
  {
    ClientWorldState world;
    SpawnTestEntity(world, 10, SpaceObjectCategory::Ship, { 0, 0, 0 });
    SpawnTestEntity(world, 20, SpaceObjectCategory::Ship, { 10, 0, 0 });

    TargetingUI targeting;
    targeting.Initialize(&world, nullptr);
    targeting.RefreshTargetList();

    // First CyclePrev wraps from -1: (-1-1+2)%2 = 0 → first element
    auto first = targeting.CyclePrev();
    Assert::AreEqual(10u, first.m_id);

    // Second CyclePrev wraps to last: (0-1+2)%2 = 1 → second element
    auto prev = targeting.CyclePrev();
    Assert::AreEqual(20u, prev.m_id);
  }

  TEST_METHOD(SetTarget_UpdatesCycleIndex)
  {
    ClientWorldState world;
    SpawnTestEntity(world, 10, SpaceObjectCategory::Ship, { 0, 0, 0 });
    SpawnTestEntity(world, 20, SpaceObjectCategory::Ship, { 10, 0, 0 });

    TargetingUI targeting;
    targeting.Initialize(&world, nullptr);
    targeting.RefreshTargetList();

    EntityHandle h;
    h.m_id = 20;
    targeting.SetTarget(h);

    Assert::AreEqual(20u, targeting.GetTarget().m_id);
    Assert::AreEqual(1, targeting.CycleIndex());
  }

  TEST_METHOD(ClearTarget_ResetsState)
  {
    ClientWorldState world;
    SpawnTestEntity(world, 10, SpaceObjectCategory::Ship, { 0, 0, 0 });

    TargetingUI targeting;
    targeting.Initialize(&world, nullptr);
    targeting.RefreshTargetList();
    (void)targeting.CycleNext();

    Assert::IsTrue(targeting.GetTarget().IsValid());

    targeting.ClearTarget();
    Assert::IsFalse(targeting.GetTarget().IsValid());
    Assert::AreEqual(-1, targeting.CycleIndex());
  }

  TEST_METHOD(CycleNext_EmptyList_ReturnsInvalid)
  {
    ClientWorldState world;

    TargetingUI targeting;
    targeting.Initialize(&world, nullptr);
    targeting.RefreshTargetList();

    auto result = targeting.CycleNext();
    Assert::IsFalse(result.IsValid());
  }
};

// ═══════════════════════════════════════════════════════════════════════════
// Chat UI Tests
// ═══════════════════════════════════════════════════════════════════════════
TEST_CLASS(ChatUITests)
{
public:
  TEST_METHOD(AddMessage_StoresMessage)
  {
    ChatUI chat;
    chat.AddMessage(ChatChannel::Zone, "Alice", "Hello world");

    Assert::AreEqual(size_t(1), chat.MessageCount());
    Assert::AreEqual(std::string("Alice"), chat.GetMessages()[0].Sender);
    Assert::AreEqual(std::string("Hello world"), chat.GetMessages()[0].Text);
  }

  TEST_METHOD(AddMessage_TrimsToMax)
  {
    ChatUI chat;
    for (size_t i = 0; i < ChatUI::MAX_STORED_MESSAGES + 10; ++i)
      chat.AddMessage(ChatChannel::Zone, "Bot", std::to_string(i));

    Assert::AreEqual(ChatUI::MAX_STORED_MESSAGES, chat.MessageCount());
    // First message should be "10" (first 10 were trimmed)
    Assert::AreEqual(std::string("10"), chat.GetMessages()[0].Text);
  }

  TEST_METHOD(TypingMode_Toggle)
  {
    ChatUI chat;
    Assert::IsFalse(chat.IsTyping());

    chat.ToggleTyping();
    Assert::IsTrue(chat.IsTyping());

    chat.ToggleTyping();
    Assert::IsFalse(chat.IsTyping());
  }

  TEST_METHOD(TypeChar_AppendsToBuffer)
  {
    ChatUI chat;
    chat.ToggleTyping();
    chat.TypeChar('H');
    chat.TypeChar('i');

    Assert::AreEqual(std::string("Hi"), chat.GetInputBuffer());
  }

  TEST_METHOD(TypeChar_RejectsWhenNotTyping)
  {
    ChatUI chat;
    chat.TypeChar('X');
    Assert::IsTrue(chat.GetInputBuffer().empty());
  }

  TEST_METHOD(TypeChar_RejectsNonPrintable)
  {
    ChatUI chat;
    chat.ToggleTyping();
    chat.TypeChar('\n');
    chat.TypeChar('\t');
    chat.TypeChar(static_cast<char>(0x01));
    Assert::IsTrue(chat.GetInputBuffer().empty());
  }

  TEST_METHOD(TypeChar_MaxLength)
  {
    ChatUI chat;
    chat.ToggleTyping();
    for (size_t i = 0; i < ChatUI::MAX_INPUT_LENGTH + 10; ++i)
      chat.TypeChar('a');

    Assert::AreEqual(ChatUI::MAX_INPUT_LENGTH, chat.GetInputBuffer().size());
  }

  TEST_METHOD(Backspace_RemovesLastChar)
  {
    ChatUI chat;
    chat.ToggleTyping();
    chat.TypeChar('A');
    chat.TypeChar('B');
    chat.TypeChar('C');
    chat.Backspace();

    Assert::AreEqual(std::string("AB"), chat.GetInputBuffer());
  }

  TEST_METHOD(Backspace_EmptyBuffer_NoOp)
  {
    ChatUI chat;
    chat.ToggleTyping();
    chat.Backspace(); // Should not crash
    Assert::IsTrue(chat.GetInputBuffer().empty());
  }

  TEST_METHOD(CommitInput_ReturnsAndClears)
  {
    ChatUI chat;
    chat.ToggleTyping();
    chat.TypeChar('O');
    chat.TypeChar('K');

    std::string result = chat.CommitInput();
    Assert::AreEqual(std::string("OK"), result);
    Assert::IsTrue(chat.GetInputBuffer().empty());
    Assert::IsFalse(chat.IsTyping());
  }

  TEST_METHOD(GetVisibleMessages_ReturnsRecent)
  {
    ChatUI chat;
    for (int i = 0; i < 20; ++i)
      chat.AddMessage(ChatChannel::Zone, "Bot", std::to_string(i));

    auto visible = chat.GetVisibleMessages();
    Assert::AreEqual(ChatUI::MAX_VISIBLE_MESSAGES, visible.size());

    // Should be the most recent messages
    Assert::AreEqual(std::string("12"), visible[0]->Text);
    Assert::AreEqual(std::string("19"), visible[7]->Text);
  }

  TEST_METHOD(GetVisibleMessages_FadedMessages_Hidden)
  {
    ChatUI chat;
    chat.AddMessage(ChatChannel::Zone, "Bot", "old");

    // Age the message beyond fade time
    chat.Update(ChatUI::MESSAGE_FADE_TIME + 1.0f);

    auto visible = chat.GetVisibleMessages();
    Assert::AreEqual(size_t(0), visible.size());
  }

  TEST_METHOD(GetVisibleMessages_TypingMode_ShowsAll)
  {
    ChatUI chat;
    chat.AddMessage(ChatChannel::Zone, "Bot", "old");
    chat.Update(ChatUI::MESSAGE_FADE_TIME + 1.0f); // Faded

    chat.ToggleTyping(); // Enter typing mode

    auto visible = chat.GetVisibleMessages();
    Assert::AreEqual(size_t(1), visible.size()); // Shown despite being faded
  }
};

// ═══════════════════════════════════════════════════════════════════════════
// Fleet Panel Tests
// ═══════════════════════════════════════════════════════════════════════════
TEST_CLASS(FleetPanelTests)
{
public:
  TEST_METHOD(ComputeLayout_CorrectEntryCount)
  {
    ClientWorldState world;
    SpawnTestEntity(world, 1, SpaceObjectCategory::Ship, { 0, 0, 0 }, 100, 100, 50, 100);
    SpawnTestEntity(world, 2, SpaceObjectCategory::Ship, { 10, 0, 0 }, 80, 100, 30, 100);

    FleetPanel panel;
    panel.Initialize(&world);

    EntityHandle h1, h2;
    h1.m_id = 1;
    h2.m_id = 2;
    panel.SetFleet({ h1, h2 });

    auto entries = panel.ComputeLayout(1920, 1080);
    Assert::AreEqual(size_t(2), entries.size());
  }

  TEST_METHOD(ComputeLayout_HullFraction)
  {
    ClientWorldState world;
    SpawnTestEntity(world, 1, SpaceObjectCategory::Ship, { 0, 0, 0 }, 50, 100, 75, 100);

    FleetPanel panel;
    panel.Initialize(&world);

    EntityHandle h;
    h.m_id = 1;
    panel.SetFleet({ h });

    auto entries = panel.ComputeLayout(1920, 1080);
    Assert::AreEqual(size_t(1), entries.size());
    Assert::AreEqual(0.5f, entries[0].HullFraction, 0.001f);
    Assert::AreEqual(0.75f, entries[0].ShieldFraction, 0.001f);
  }

  TEST_METHOD(ComputeLayout_VerticalStacking)
  {
    ClientWorldState world;
    SpawnTestEntity(world, 1, SpaceObjectCategory::Ship, { 0, 0, 0 }, 100, 100);
    SpawnTestEntity(world, 2, SpaceObjectCategory::Ship, { 10, 0, 0 }, 100, 100);
    SpawnTestEntity(world, 3, SpaceObjectCategory::Ship, { 20, 0, 0 }, 100, 100);

    FleetPanel panel;
    panel.Initialize(&world);

    EntityHandle h1, h2, h3;
    h1.m_id = 1; h2.m_id = 2; h3.m_id = 3;
    panel.SetFleet({ h1, h2, h3 });

    auto entries = panel.ComputeLayout(1920, 1080);

    // Entries should be stacked vertically
    Assert::IsTrue(entries[0].SlotRect.top < entries[1].SlotRect.top);
    Assert::IsTrue(entries[1].SlotRect.top < entries[2].SlotRect.top);

    // All at same left position
    Assert::AreEqual(entries[0].SlotRect.left, entries[1].SlotRect.left);
    Assert::AreEqual(entries[1].SlotRect.left, entries[2].SlotRect.left);
  }

  TEST_METHOD(ComputeLayout_EmptyFleet)
  {
    FleetPanel panel;
    panel.Initialize(nullptr);
    panel.SetFleet({});

    auto entries = panel.ComputeLayout(1920, 1080);
    Assert::AreEqual(size_t(0), entries.size());
  }
};

// ═══════════════════════════════════════════════════════════════════════════
// Jumpgate UI Tests
// ═══════════════════════════════════════════════════════════════════════════
TEST_CLASS(JumpgateUITests)
{
public:
  TEST_METHOD(FindNearbyJumpgate_InRange)
  {
    ClientWorldState world;
    SpawnTestEntity(world, 1, SpaceObjectCategory::Ship, { 0, 0, 0 });
    SpawnTestEntity(world, 2, SpaceObjectCategory::Jumpgate, { 50, 0, 0 });

    JumpgateUI ui;
    ui.Initialize(&world, nullptr);
    ui.WarpRange = 100.0f;

    EntityHandle ship;
    ship.m_id = 1;
    auto gate = ui.FindNearbyJumpgate(ship);
    Assert::IsTrue(gate.IsValid());
    Assert::AreEqual(2u, gate.m_id);
  }

  TEST_METHOD(FindNearbyJumpgate_OutOfRange)
  {
    ClientWorldState world;
    SpawnTestEntity(world, 1, SpaceObjectCategory::Ship, { 0, 0, 0 });
    SpawnTestEntity(world, 2, SpaceObjectCategory::Jumpgate, { 500, 0, 0 });

    JumpgateUI ui;
    ui.Initialize(&world, nullptr);
    ui.WarpRange = 100.0f;

    EntityHandle ship;
    ship.m_id = 1;
    auto gate = ui.FindNearbyJumpgate(ship);
    Assert::IsFalse(gate.IsValid());
  }

  TEST_METHOD(Update_SetsWarpAvailable)
  {
    ClientWorldState world;
    SpawnTestEntity(world, 1, SpaceObjectCategory::Ship, { 0, 0, 0 });
    SpawnTestEntity(world, 2, SpaceObjectCategory::Jumpgate, { 30, 0, 0 });

    JumpgateUI ui;
    ui.Initialize(&world, nullptr);
    ui.WarpRange = 100.0f;

    EntityHandle ship;
    ship.m_id = 1;
    ui.Update(ship);

    Assert::IsTrue(ui.IsWarpAvailable());
    Assert::AreEqual(2u, ui.NearbyJumpgate().m_id);
  }

  TEST_METHOD(FindNearbyJumpgate_SelectsNearest)
  {
    ClientWorldState world;
    SpawnTestEntity(world, 1, SpaceObjectCategory::Ship, { 0, 0, 0 });
    SpawnTestEntity(world, 2, SpaceObjectCategory::Jumpgate, { 80, 0, 0 });
    SpawnTestEntity(world, 3, SpaceObjectCategory::Jumpgate, { 30, 0, 0 });

    JumpgateUI ui;
    ui.Initialize(&world, nullptr);
    ui.WarpRange = 100.0f;

    EntityHandle ship;
    ship.m_id = 1;
    auto gate = ui.FindNearbyJumpgate(ship);

    Assert::AreEqual(3u, gate.m_id); // Closer gate
  }
};

// ═══════════════════════════════════════════════════════════════════════════
// Ship Status Message Tests
// ═══════════════════════════════════════════════════════════════════════════
TEST_CLASS(ShipStatusMsgTests)
{
public:
  TEST_METHOD(ShipStatusMsg_RoundTrip)
  {
    ShipStatusMsg original;
    ShipStatusEntry entry;
    entry.HandleId    = 42;
    entry.ShieldHP    = 80.0f;
    entry.ShieldMaxHP = 100.0f;
    entry.ArmorHP     = 60.0f;
    entry.ArmorMaxHP  = 100.0f;
    entry.HullHP      = 90.0f;
    entry.HullMaxHP   = 150.0f;
    entry.Energy      = 50.0f;
    entry.EnergyMax   = 75.0f;
    entry.Speed       = 25.0f;
    entry.MaxSpeed    = 100.0f;
    entry.FactionId   = 3;
    original.Ships.push_back(entry);

    DataWriter writer;
    original.Write(writer);

    DataReader reader(reinterpret_cast<const uint8_t*>(writer.Data()),
      static_cast<size_t>(writer.Size()));
    ShipStatusMsg parsed;
    parsed.Read(reader);

    Assert::AreEqual(size_t(1), parsed.Ships.size());
    Assert::AreEqual(42u, parsed.Ships[0].HandleId);
    Assert::AreEqual(80.0f, parsed.Ships[0].ShieldHP, 0.01f);
    Assert::AreEqual(100.0f, parsed.Ships[0].ShieldMaxHP, 0.01f);
    Assert::AreEqual(60.0f, parsed.Ships[0].ArmorHP, 0.01f);
    Assert::AreEqual(90.0f, parsed.Ships[0].HullHP, 0.01f);
    Assert::AreEqual(150.0f, parsed.Ships[0].HullMaxHP, 0.01f);
    Assert::AreEqual(50.0f, parsed.Ships[0].Energy, 0.01f);
    Assert::AreEqual(75.0f, parsed.Ships[0].EnergyMax, 0.01f);
    Assert::AreEqual(25.0f, parsed.Ships[0].Speed, 0.01f);
    Assert::AreEqual(100.0f, parsed.Ships[0].MaxSpeed, 0.01f);
    Assert::AreEqual(static_cast<uint16_t>(3), parsed.Ships[0].FactionId);
  }

  TEST_METHOD(PlayerInfoMsg_RoundTrip)
  {
    PlayerInfoMsg original;
    original.FlagshipHandle.m_id = 99;

    DataWriter writer;
    original.Write(writer);

    DataReader reader(reinterpret_cast<const uint8_t*>(writer.Data()),
      static_cast<size_t>(writer.Size()));
    PlayerInfoMsg parsed;
    parsed.Read(reader);

    Assert::AreEqual(99u, parsed.FlagshipHandle.m_id);
  }

  TEST_METHOD(ApplyShipStatus_UpdatesClientEntity)
  {
    ClientWorldState world;
    SpawnTestEntity(world, 5, SpaceObjectCategory::Ship, { 0, 0, 0 });

    ShipStatusMsg status;
    ShipStatusEntry entry;
    entry.HandleId    = 5;
    entry.HullHP      = 75.0f;
    entry.HullMaxHP   = 100.0f;
    entry.ShieldHP    = 50.0f;
    entry.ShieldMaxHP = 80.0f;
    entry.FactionId   = 2;
    status.Ships.push_back(entry);

    world.ApplyShipStatus(status);

    EntityHandle h;
    h.m_id = 5;
    const ClientEntity* ent = world.GetEntity(h);
    Assert::IsNotNull(ent);
    Assert::IsTrue(ent->HasShipStats);
    Assert::AreEqual(75.0f, ent->HullHP, 0.01f);
    Assert::AreEqual(100.0f, ent->HullMaxHP, 0.01f);
    Assert::AreEqual(50.0f, ent->ShieldHP, 0.01f);
    Assert::AreEqual(static_cast<uint16_t>(2), ent->FactionId);
  }
};

// ═══════════════════════════════════════════════════════════════════════════
// Client World State — Flagship Tests
// ═══════════════════════════════════════════════════════════════════════════
TEST_CLASS(FlagshipTests)
{
public:
  TEST_METHOD(SetFlagship_GetFlagship)
  {
    ClientWorldState world;
    EntityHandle h;
    h.m_id = 42;
    world.SetFlagship(h);
    Assert::AreEqual(42u, world.GetFlagship().m_id);
  }

  TEST_METHOD(DefaultFlagship_IsInvalid)
  {
    ClientWorldState world;
    Assert::IsFalse(world.GetFlagship().IsValid());
  }
};
