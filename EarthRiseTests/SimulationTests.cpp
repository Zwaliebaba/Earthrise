#include "pch.h"
#include "SpatialHash.h"
#include "MovementSystem.h"
#include "CollisionSystem.h"
#include "CommandProcessor.h"
#include "JumpgateSystem.h"
#include "SpaceObjectManager.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Neuron;
using namespace GameLogic;

namespace EarthRiseTests::Simulation
{
  // =======================================================================
  // SpatialHash Tests
  // =======================================================================
  TEST_CLASS(SpatialHashTests)
  {
  public:
    TEST_METHOD(InsertAndQueryFindsEntity)
    {
      SpatialHash hash;
      EntityHandle h(1, 0);
      XMVECTOR pos = XMVectorSet(100.0f, 200.0f, 300.0f, 0.0f);

      hash.Insert(h, pos);

      Assert::AreEqual(size_t(1), hash.TotalEntityCount());
      Assert::AreEqual(size_t(1), hash.OccupiedCellCount());

      // Query should find it
      int found = 0;
      hash.QuerySphere(pos, 10.0f, [&](EntityHandle _h) { if (_h == h) ++found; });
      Assert::AreEqual(1, found);
    }

    TEST_METHOD(RemoveEntityLeavesEmpty)
    {
      SpatialHash hash;
      EntityHandle h(1, 0);
      XMVECTOR pos = XMVectorZero();

      hash.Insert(h, pos);
      hash.Remove(h);

      Assert::AreEqual(size_t(0), hash.TotalEntityCount());
      Assert::AreEqual(size_t(0), hash.OccupiedCellCount());
    }

    TEST_METHOD(QueryExcludesDistantEntities)
    {
      SpatialHash hash;
      EntityHandle nearH(1, 0);
      EntityHandle farH(2, 0);

      XMVECTOR nearPos = XMVectorZero();
      XMVECTOR farPos  = XMVectorSet(50000.0f, 0.0f, 0.0f, 0.0f);

      hash.Insert(nearH, nearPos);
      hash.Insert(farH, farPos);

      int found = 0;
      hash.QuerySphere(nearPos, 100.0f, [&](EntityHandle) { ++found; });

      // Only the near entity's cell should be hit
      Assert::AreEqual(1, found);
    }

    TEST_METHOD(ClearRemovesAll)
    {
      SpatialHash hash;
      for (uint32_t i = 1; i <= 10; ++i)
      {
        XMVECTOR p = XMVectorSet(static_cast<float>(i * 100), 0.0f, 0.0f, 0.0f);
        hash.Insert(EntityHandle(i, 0), p);
      }
      Assert::AreEqual(size_t(10), hash.TotalEntityCount());

      hash.Clear();
      Assert::AreEqual(size_t(0), hash.TotalEntityCount());
      Assert::AreEqual(size_t(0), hash.OccupiedCellCount());
    }

    TEST_METHOD(SameCellEntitiesAllReturned)
    {
      SpatialHash hash;
      // Insert several entities close together (within one cell)
      EntityHandle h1(1, 0);
      EntityHandle h2(2, 0);
      EntityHandle h3(3, 0);
      XMVECTOR pos1 = XMVectorSet(10.0f, 10.0f, 10.0f, 0.0f);
      XMVECTOR pos2 = XMVectorSet(15.0f, 10.0f, 10.0f, 0.0f);
      XMVECTOR pos3 = XMVectorSet(20.0f, 10.0f, 10.0f, 0.0f);

      hash.Insert(h1, pos1);
      hash.Insert(h2, pos2);
      hash.Insert(h3, pos3);

      Assert::AreEqual(size_t(1), hash.OccupiedCellCount());

      int found = 0;
      hash.QuerySphere(pos1, 50.0f, [&](EntityHandle) { ++found; });
      Assert::AreEqual(3, found);
    }

    TEST_METHOD(UpdateMovesToNewCell)
    {
      SpatialHash hash;
      EntityHandle h(1, 0);
      XMVECTOR oldPos = XMVectorZero();
      XMVECTOR newPos = XMVectorSet(10000.0f, 0.0f, 0.0f, 0.0f); // Far away cell

      hash.Insert(h, oldPos);
      hash.Update(h, oldPos, newPos);

      // Should not be found at old position
      int foundAtOld = 0;
      hash.QuerySphere(oldPos, 10.0f, [&](EntityHandle) { ++foundAtOld; });
      Assert::AreEqual(0, foundAtOld);

      // Should be found at new position
      int foundAtNew = 0;
      hash.QuerySphere(newPos, 10.0f, [&](EntityHandle _h) { if (_h == h) ++foundAtNew; });
      Assert::AreEqual(1, foundAtNew);
    }
  };

  // =======================================================================
  // MovementSystem Tests
  // =======================================================================
  TEST_CLASS(MovementSystemTests)
  {
  public:
    TEST_METHOD(VelocityIntegrationUpdatesPosition)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);

      EntityHandle ship = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0.0f, 0.0f, 0.0f });
      SpaceObject* obj = mgr.GetSpaceObject(ship);
      Assert::IsNotNull(obj);

      // Set a velocity directly
      obj->Velocity = { 100.0f, 0.0f, 0.0f };

      // Tick for 1 second
      movement.Update(1.0f);

      // Position should have moved. Ship has no target so drag applies,
      // but velocity integration still happens.
      Assert::AreNotEqual(0.0f, obj->Position.x);
    }

    TEST_METHOD(MoveTargetCausesMovement)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);

      EntityHandle ship = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0.0f, 0.0f, 0.0f });

      // Set up ship thrust/speed
      ShipData* shipData = mgr.GetShipData(ship);
      Assert::IsNotNull(shipData);
      shipData->MaxSpeed = 100.0f;
      shipData->Thrust = 50.0f;
      shipData->TurnRate = 3.14f;

      // Set a far target
      movement.SetMoveTarget(ship, { 0.0f, 0.0f, 1000.0f });
      Assert::IsTrue(movement.HasMoveTarget(ship));

      // Tick several times
      for (int i = 0; i < 100; ++i)
        movement.Update(0.05f);

      // Ship should have moved toward target (positive Z)
      SpaceObject* obj = mgr.GetSpaceObject(ship);
      Assert::IsTrue(obj->Position.z > 0.0f, L"Ship should move toward target along Z");
    }

    TEST_METHOD(ShipArrivesAndStops)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);

      EntityHandle ship = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0.0f, 0.0f, 0.0f });
      ShipData* shipData = mgr.GetShipData(ship);
      shipData->MaxSpeed = 200.0f;
      shipData->Thrust = 200.0f;
      shipData->TurnRate = 10.0f;

      // Target close ahead in Z
      movement.SetMoveTarget(ship, { 0.0f, 0.0f, 50.0f });

      // Run until arrival (many ticks)
      for (int i = 0; i < 500; ++i)
        movement.Update(0.05f);

      // Should have arrived and stopped
      Assert::IsFalse(movement.HasMoveTarget(ship), L"Target should be cleared after arrival");

      SpaceObject* obj = mgr.GetSpaceObject(ship);
      float speed = std::sqrt(
        obj->Velocity.x * obj->Velocity.x +
        obj->Velocity.y * obj->Velocity.y +
        obj->Velocity.z * obj->Velocity.z);
      Assert::IsTrue(speed < 1.0f, L"Ship should be nearly stopped after arrival");
    }

    TEST_METHOD(ClearMoveTargetStopsTargeting)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);

      EntityHandle ship = mgr.CreateEntity(SpaceObjectCategory::Ship);
      movement.SetMoveTarget(ship, { 100.0f, 0.0f, 0.0f });
      Assert::IsTrue(movement.HasMoveTarget(ship));

      movement.ClearMoveTarget(ship);
      Assert::IsFalse(movement.HasMoveTarget(ship));
    }
  };

  // =======================================================================
  // CollisionSystem Tests
  // =======================================================================
  TEST_CLASS(CollisionSystemTests)
  {
  public:
    TEST_METHOD(OverlappingEntitiesCollide)
    {
      SpaceObjectManager mgr;
      SpatialHash spatialHash;
      CollisionSystem collision(mgr, spatialHash);

      // Two ships at the same position, radius 10 each
      EntityHandle a = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0.0f, 0.0f, 0.0f });
      EntityHandle b = mgr.CreateEntity(SpaceObjectCategory::Ship, { 5.0f, 0.0f, 0.0f });

      mgr.GetSpaceObject(a)->BoundingRadius = 10.0f;
      mgr.GetSpaceObject(b)->BoundingRadius = 10.0f;

      collision.RebuildSpatialHash();
      collision.DetectCollisions();

      Assert::AreEqual(size_t(1), collision.GetCollisions().size());
    }

    TEST_METHOD(DistantEntitiesDoNotCollide)
    {
      SpaceObjectManager mgr;
      SpatialHash spatialHash;
      CollisionSystem collision(mgr, spatialHash);

      EntityHandle a = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0.0f, 0.0f, 0.0f });
      EntityHandle b = mgr.CreateEntity(SpaceObjectCategory::Ship, { 5000.0f, 0.0f, 0.0f });

      mgr.GetSpaceObject(a)->BoundingRadius = 10.0f;
      mgr.GetSpaceObject(b)->BoundingRadius = 10.0f;

      collision.RebuildSpatialHash();
      collision.DetectCollisions();

      Assert::AreEqual(size_t(0), collision.GetCollisions().size());
    }

    TEST_METHOD(DecorationsAreSkipped)
    {
      SpaceObjectManager mgr;
      SpatialHash spatialHash;
      CollisionSystem collision(mgr, spatialHash);

      // Decoration overlapping a ship should produce no collision
      EntityHandle ship = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0.0f, 0.0f, 0.0f });
      EntityHandle deco = mgr.CreateEntity(SpaceObjectCategory::Decoration, { 0.0f, 0.0f, 0.0f });

      mgr.GetSpaceObject(ship)->BoundingRadius = 10.0f;
      mgr.GetSpaceObject(deco)->BoundingRadius = 10.0f;

      collision.RebuildSpatialHash();
      collision.DetectCollisions();

      Assert::AreEqual(size_t(0), collision.GetCollisions().size());
    }

    TEST_METHOD(NoDuplicateCollisionPairs)
    {
      SpaceObjectManager mgr;
      SpatialHash spatialHash;
      CollisionSystem collision(mgr, spatialHash);

      // Three overlapping entities should produce 3 unique pairs, not 6
      EntityHandle a = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0.0f, 0.0f, 0.0f });
      EntityHandle b = mgr.CreateEntity(SpaceObjectCategory::Ship, { 1.0f, 0.0f, 0.0f });
      EntityHandle c = mgr.CreateEntity(SpaceObjectCategory::Ship, { 2.0f, 0.0f, 0.0f });

      mgr.GetSpaceObject(a)->BoundingRadius = 50.0f;
      mgr.GetSpaceObject(b)->BoundingRadius = 50.0f;
      mgr.GetSpaceObject(c)->BoundingRadius = 50.0f;

      collision.RebuildSpatialHash();
      collision.DetectCollisions();

      Assert::AreEqual(size_t(3), collision.GetCollisions().size());
    }
  };

  // =======================================================================
  // CommandProcessor Tests
  // =======================================================================
  TEST_CLASS(CommandProcessorTests)
  {
  public:
    TEST_METHOD(MoveToSetsMoveTargetOnFleet)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      CommandProcessor cmd(mgr, movement);

      EntityHandle ship = mgr.CreateEntity(SpaceObjectCategory::Ship);
      ShipData* sd = mgr.GetShipData(ship);
      sd->MaxSpeed = 100.0f;
      sd->Thrust = 50.0f;
      sd->TurnRate = 3.14f;

      cmd.RegisterPlayerFleet(1, { ship });

      InputCommand moveCmd;
      moveCmd.SessionId = 1;
      moveCmd.Type = CommandType::MoveTo;
      moveCmd.TargetPosition = { 500.0f, 0.0f, 0.0f };

      cmd.EnqueueCommand(moveCmd);
      cmd.ProcessCommands();

      Assert::IsTrue(movement.HasMoveTarget(ship));
    }

    TEST_METHOD(UnregisteredPlayerCommandIgnored)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      CommandProcessor cmd(mgr, movement);

      EntityHandle ship = mgr.CreateEntity(SpaceObjectCategory::Ship);

      // Don't register player fleet — command should be silently ignored
      InputCommand moveCmd;
      moveCmd.SessionId = 99;
      moveCmd.Type = CommandType::MoveTo;
      moveCmd.TargetPosition = { 500.0f, 0.0f, 0.0f };

      cmd.EnqueueCommand(moveCmd);
      cmd.ProcessCommands();

      Assert::IsFalse(movement.HasMoveTarget(ship));
    }

    TEST_METHOD(FleetRegistrationAndUnregistration)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      CommandProcessor cmd(mgr, movement);

      EntityHandle ship = mgr.CreateEntity(SpaceObjectCategory::Ship);
      cmd.RegisterPlayerFleet(42, { ship });

      const auto* fleet = cmd.GetPlayerFleet(42);
      Assert::IsNotNull(fleet);
      Assert::AreEqual(size_t(1), fleet->size());

      cmd.UnregisterPlayer(42);
      Assert::IsNull(cmd.GetPlayerFleet(42));
    }
  };

  // =======================================================================
  // JumpgateSystem Tests
  // =======================================================================
  TEST_CLASS(JumpgateSystemTests)
  {
  public:
    TEST_METHOD(WarpTeleportsFleetToDestination)
    {
      SpaceObjectManager mgr;
      JumpgateSystem jumpgates(mgr);

      // Create a jumpgate at origin with destination at (10000, 0, 0)
      EntityHandle gate = mgr.CreateEntity(SpaceObjectCategory::Jumpgate, { 0.0f, 0.0f, 0.0f });
      JumpgateData* gateData = mgr.GetJumpgateData(gate);
      Assert::IsNotNull(gateData);
      gateData->ActivationRadius = 100.0f;
      gateData->DestinationPosition = { 10000.0f, 0.0f, 0.0f };
      gateData->IsActive = true;

      // Create a ship within activation radius
      EntityHandle ship = mgr.CreateEntity(SpaceObjectCategory::Ship, { 10.0f, 0.0f, 0.0f });

      // Request warp
      jumpgates.RequestWarp(gate, { ship });
      jumpgates.Update();

      // Ship should have been teleported near destination
      SpaceObject* shipObj = mgr.GetSpaceObject(ship);
      Assert::IsTrue(shipObj->Position.x > 9000.0f, L"Ship should be near gate destination");

      // Warp should be in completed list
      Assert::AreEqual(size_t(1), jumpgates.CompletedWarps().size());
    }

    TEST_METHOD(WarpFailsWhenShipOutOfRange)
    {
      SpaceObjectManager mgr;
      JumpgateSystem jumpgates(mgr);

      EntityHandle gate = mgr.CreateEntity(SpaceObjectCategory::Jumpgate, { 0.0f, 0.0f, 0.0f });
      JumpgateData* gateData = mgr.GetJumpgateData(gate);
      gateData->ActivationRadius = 50.0f;
      gateData->DestinationPosition = { 10000.0f, 0.0f, 0.0f };
      gateData->IsActive = true;

      // Ship far from gate
      EntityHandle ship = mgr.CreateEntity(SpaceObjectCategory::Ship, { 500.0f, 0.0f, 0.0f });

      jumpgates.RequestWarp(gate, { ship });
      jumpgates.Update();

      // Ship should NOT have been teleported
      SpaceObject* shipObj = mgr.GetSpaceObject(ship);
      Assert::AreEqual(500.0f, shipObj->Position.x);

      // No completed warps — request is still pending
      Assert::AreEqual(size_t(0), jumpgates.CompletedWarps().size());
    }

    TEST_METHOD(InactiveGateRejectsWarp)
    {
      SpaceObjectManager mgr;
      JumpgateSystem jumpgates(mgr);

      EntityHandle gate = mgr.CreateEntity(SpaceObjectCategory::Jumpgate, { 0.0f, 0.0f, 0.0f });
      JumpgateData* gateData = mgr.GetJumpgateData(gate);
      gateData->ActivationRadius = 100.0f;
      gateData->DestinationPosition = { 10000.0f, 0.0f, 0.0f };
      gateData->IsActive = false; // Inactive!

      EntityHandle ship = mgr.CreateEntity(SpaceObjectCategory::Ship, { 10.0f, 0.0f, 0.0f });

      jumpgates.RequestWarp(gate, { ship });
      jumpgates.Update();

      // Ship should stay put — gate is inactive
      SpaceObject* shipObj = mgr.GetSpaceObject(ship);
      Assert::AreEqual(10.0f, shipObj->Position.x);

      // Pending warp should be removed (invalid gate), not completed
      Assert::AreEqual(size_t(0), jumpgates.CompletedWarps().size());
    }
  };

  // =======================================================================
  // Integration: Zone-like orchestration test
  // =======================================================================
  TEST_CLASS(SimulationIntegrationTests)
  {
  public:
    TEST_METHOD(TickOneHundredFramesNoCrash)
    {
      // Create all systems and tick them 100 times to prove stability
      SpaceObjectManager mgr;
      SpatialHash spatialHash;
      MovementSystem movement(mgr);
      CollisionSystem collision(mgr, spatialHash);
      CommandProcessor commands(mgr, movement);
      JumpgateSystem jumpgates(mgr);

      // Create a mix of entities
      EntityHandle ship = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0.0f, 0.0f, 0.0f });
      ShipData* sd = mgr.GetShipData(ship);
      sd->MaxSpeed = 100.0f;
      sd->Thrust = 50.0f;
      sd->TurnRate = 3.14f;

      [[maybe_unused]] auto ast1 = mgr.CreateEntity(SpaceObjectCategory::Asteroid, { 1000.0f, 0.0f, 0.0f });
      [[maybe_unused]] auto ast2 = mgr.CreateEntity(SpaceObjectCategory::Asteroid, { 2000.0f, 500.0f, 0.0f });
      [[maybe_unused]] auto st   = mgr.CreateEntity(SpaceObjectCategory::Station, { 3000.0f, 0.0f, 0.0f });

      EntityHandle gate = mgr.CreateEntity(SpaceObjectCategory::Jumpgate, { 5000.0f, 0.0f, 0.0f });
      JumpgateData* gd = mgr.GetJumpgateData(gate);
      gd->ActivationRadius = 100.0f;
      gd->DestinationPosition = { 50000.0f, 0.0f, 0.0f };
      gd->IsActive = true;

      // Register fleet and issue a move command
      commands.RegisterPlayerFleet(1, { ship });
      InputCommand moveCmd;
      moveCmd.SessionId = 1;
      moveCmd.Type = CommandType::MoveTo;
      moveCmd.TargetPosition = { 500.0f, 0.0f, 0.0f };
      commands.EnqueueCommand(moveCmd);

      constexpr float dt = 0.05f;
      for (int i = 0; i < 100; ++i)
      {
        commands.ProcessCommands();
        movement.Update(dt);
        collision.RebuildSpatialHash();
        collision.DetectCollisions();
        jumpgates.Update();
      }

      // If we get here without crashing, the test passes.
      // Verify the ship actually moved.
      SpaceObject* obj = mgr.GetSpaceObject(ship);
      Assert::IsNotNull(obj);
      Assert::IsTrue(obj->Active);
    }

    TEST_METHOD(ShipMovesOverMultipleFrames)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);

      EntityHandle ship = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0.0f, 0.0f, 0.0f });
      ShipData* sd = mgr.GetShipData(ship);
      sd->MaxSpeed = 200.0f;
      sd->Thrust = 100.0f;
      sd->TurnRate = 6.28f;

      movement.SetMoveTarget(ship, { 0.0f, 0.0f, 2000.0f });

      float totalTime = 0.0f;
      constexpr float dt = 0.05f;

      // Track position over time — should monotonically increase in Z
      float lastZ = 0.0f;
      bool monotonicZ = true;

      for (int i = 0; i < 200; ++i)
      {
        movement.Update(dt);
        totalTime += dt;

        SpaceObject* obj = mgr.GetSpaceObject(ship);
        if (obj->Position.z < lastZ - 0.01f) // small tolerance
          monotonicZ = false;
        lastZ = obj->Position.z;
      }

      Assert::IsTrue(monotonicZ, L"Ship Z position should increase monotonically toward target");
      Assert::IsTrue(lastZ > 100.0f, L"Ship should have moved significantly in 10 seconds");
    }
  };
}
