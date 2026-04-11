#include "pch.h"
#include "CombatSystem.h"
#include "ProjectileSystem.h"
#include "LootSystem.h"
#include "DockingSystem.h"
#include "NpcAI.h"
#include "FleetSystem.h"
#include "CollisionSystem.h"
#include "SpaceObjectManager.h"
#include "MovementSystem.h"
#include "SpatialHash.h"
#include "GameTypes/Fleet.h"
#include "GameTypes/ShipLoadout.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Neuron;
using namespace GameLogic;

namespace EarthRiseTests::Gameplay
{
  // Helper: create a ship with HP and turret at a given position.
  static EntityHandle CreateArmedShip(SpaceObjectManager& _mgr, const XMFLOAT3& _pos,
    float _shieldHP, float _armorHP, float _hullHP, uint16_t _factionId = 0)
  {
    auto handle = _mgr.CreateEntity(SpaceObjectCategory::Ship, _pos, 0, 0);
    ShipData* ship = _mgr.GetShipData(handle);
    ship->ShieldHP = _shieldHP;
    ship->ShieldMaxHP = _shieldHP;
    ship->ArmorHP = _armorHP;
    ship->ArmorMaxHP = _armorHP;
    ship->HullHP = _hullHP;
    ship->HullMaxHP = _hullHP;
    ship->MaxSpeed = 50.0f;
    ship->TurnRate = 2.0f;
    ship->Thrust = 20.0f;
    ship->FactionId = _factionId;

    // Create and attach a turret.
    auto turretHandle = _mgr.CreateEntity(SpaceObjectCategory::Turret, _pos, 0, 0);
    TurretData* turret = _mgr.GetTurretData(turretHandle);
    turret->ParentShip = handle;
    turret->FireRate = 2.0f;     // 2 shots/sec
    turret->Damage = 25.0f;
    turret->Range = 100.0f;
    turret->ProjectileSpeed = 200.0f;
    turret->CooldownTimer = 0.0f;
    turret->TrackingSpeed = 3.0f;

    ship->TurretSlots[0] = turretHandle;
    ship->TurretSlotCount = 1;

    return handle;
  }

  // =======================================================================
  // CombatSystem Tests
  // =======================================================================
  TEST_CLASS(CombatSystemTests)
  {
  public:
    TEST_METHOD(ProjectileHitsShipReducesHP)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      CombatSystem combat(mgr, movement);

      // Create a ship with known HP.
      auto shipHandle = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0, 0, 0 }, 0, 0);
      ShipData* ship = mgr.GetShipData(shipHandle);
      ship->ShieldHP = 50.0f;
      ship->ShieldMaxHP = 50.0f;
      ship->ArmorHP = 50.0f;
      ship->ArmorMaxHP = 50.0f;
      ship->HullHP = 100.0f;
      ship->HullMaxHP = 100.0f;

      // Create a projectile at the same position (overlapping = collision).
      auto projHandle = mgr.CreateEntity(SpaceObjectCategory::Projectile, { 0, 0, 0 }, 0, 0);
      ProjectileData* proj = mgr.GetProjectileData(projHandle);
      proj->Damage = 30.0f;
      proj->Source = EntityHandle::Invalid(); // Not from the ship itself
      proj->Lifetime = 5.0f;

      // Simulate a collision event.
      std::vector<CollisionEvent> collisions = { { projHandle, shipHandle } };
      combat.ProcessCollisions(collisions);

      // Shield should absorb 30 damage.
      Assert::AreEqual(20.0f, ship->ShieldHP, 0.01f, L"Shield should be reduced by 30");
      Assert::AreEqual(50.0f, ship->ArmorHP, 0.01f, L"Armor untouched");
      Assert::AreEqual(100.0f, ship->HullHP, 0.01f, L"Hull untouched");

      // Projectile should be in hits list.
      Assert::AreEqual(size_t(1), combat.GetProjectileHits().size());
    }

    TEST_METHOD(DamageOverflowsFromShieldToArmor)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      CombatSystem combat(mgr, movement);

      auto shipHandle = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0, 0, 0 }, 0, 0);
      ShipData* ship = mgr.GetShipData(shipHandle);
      ship->ShieldHP = 10.0f;
      ship->ShieldMaxHP = 10.0f;
      ship->ArmorHP = 50.0f;
      ship->ArmorMaxHP = 50.0f;
      ship->HullHP = 100.0f;
      ship->HullMaxHP = 100.0f;

      auto projHandle = mgr.CreateEntity(SpaceObjectCategory::Projectile, { 0, 0, 0 }, 0, 0);
      ProjectileData* proj = mgr.GetProjectileData(projHandle);
      proj->Damage = 40.0f;
      proj->Source = EntityHandle::Invalid();
      proj->Lifetime = 5.0f;

      std::vector<CollisionEvent> collisions = { { projHandle, shipHandle } };
      combat.ProcessCollisions(collisions);

      Assert::AreEqual(0.0f, ship->ShieldHP, 0.01f);
      Assert::AreEqual(20.0f, ship->ArmorHP, 0.01f, L"Armor = 50 - (40-10) = 20");
      Assert::AreEqual(100.0f, ship->HullHP, 0.01f);
    }

    TEST_METHOD(ShipDestroyedWhenHullReachesZero)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      CombatSystem combat(mgr, movement);

      auto shipHandle = mgr.CreateEntity(SpaceObjectCategory::Ship, { 10, 20, 30 }, 0, 0);
      ShipData* ship = mgr.GetShipData(shipHandle);
      ship->ShieldHP = 0.0f;
      ship->ArmorHP = 0.0f;
      ship->HullHP = 5.0f;
      ship->HullMaxHP = 100.0f;

      auto projHandle = mgr.CreateEntity(SpaceObjectCategory::Projectile, { 10, 20, 30 }, 0, 0);
      ProjectileData* proj = mgr.GetProjectileData(projHandle);
      proj->Damage = 50.0f;
      proj->Source = EntityHandle::Invalid();
      proj->Lifetime = 5.0f;

      std::vector<CollisionEvent> collisions = { { projHandle, shipHandle } };
      combat.ProcessCollisions(collisions);

      Assert::AreEqual(size_t(1), combat.GetDestroyedShips().size());
      Assert::IsTrue(combat.GetDestroyedShips()[0].Destroyed == shipHandle);
    }

    TEST_METHOD(FriendlyFireIgnored)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      CombatSystem combat(mgr, movement);

      auto shipHandle = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0, 0, 0 }, 0, 0);
      ShipData* ship = mgr.GetShipData(shipHandle);
      ship->ShieldHP = 100.0f;
      ship->ShieldMaxHP = 100.0f;
      ship->HullHP = 100.0f;
      ship->HullMaxHP = 100.0f;

      auto projHandle = mgr.CreateEntity(SpaceObjectCategory::Projectile, { 0, 0, 0 }, 0, 0);
      ProjectileData* proj = mgr.GetProjectileData(projHandle);
      proj->Damage = 50.0f;
      proj->Source = shipHandle; // Fired by the same ship
      proj->Lifetime = 5.0f;

      std::vector<CollisionEvent> collisions = { { projHandle, shipHandle } };
      combat.ProcessCollisions(collisions);

      // No damage because source == target ship.
      Assert::AreEqual(100.0f, ship->ShieldHP, 0.01f, L"No self-damage");
      Assert::AreEqual(size_t(0), combat.GetDestroyedShips().size());
    }

    TEST_METHOD(TurretFiresWhenTargetInRange)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      CombatSystem combat(mgr, movement);

      // Ship A with turret attacking Ship B.
      auto shipA = CreateArmedShip(mgr, { 0, 0, 0 }, 100, 100, 100);
      auto shipB = mgr.CreateEntity(SpaceObjectCategory::Ship, { 10, 0, 0 }, 0, 0);
      ShipData* shipBData = mgr.GetShipData(shipB);
      shipBData->HullHP = 100.0f;
      shipBData->HullMaxHP = 100.0f;

      combat.SetAttackTarget(shipA, shipB);

      uint32_t initialCount = mgr.ActiveCount();
      combat.Update(0.05f); // One tick

      // A projectile should have been spawned.
      Assert::IsTrue(mgr.ActiveCount() > initialCount, L"Projectile should be created");
    }
  };

  // =======================================================================
  // ProjectileSystem Tests
  // =======================================================================
  TEST_CLASS(ProjectileSystemTests)
  {
  public:
    TEST_METHOD(ProjectileDespawnsAfterLifetime)
    {
      SpaceObjectManager mgr;
      ProjectileSystem projectiles(mgr);

      auto projHandle = mgr.CreateEntity(SpaceObjectCategory::Projectile, { 0, 0, 0 }, 0, 0);
      ProjectileData* proj = mgr.GetProjectileData(projHandle);
      proj->Lifetime = 1.0f;
      proj->Speed = 100.0f;

      // Tick 0.5s — still alive.
      projectiles.Update(0.5f);
      Assert::AreEqual(size_t(0), projectiles.GetExpired().size());

      // Tick 0.6s — expired (total 1.1s > 1.0s lifetime).
      projectiles.Update(0.6f);
      Assert::AreEqual(size_t(1), projectiles.GetExpired().size());
      Assert::IsTrue(projectiles.GetExpired()[0] == projHandle);
    }

    TEST_METHOD(ProjectileTravelsAtSpeed)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);

      auto projHandle = mgr.CreateEntity(SpaceObjectCategory::Projectile, { 0, 0, 0 }, 0, 0);
      ProjectileData* proj = mgr.GetProjectileData(projHandle);
      proj->Lifetime = 5.0f;
      proj->Speed = 100.0f;
      proj->TurnRate = 0.0f; // Unguided

      SpaceObject* obj = mgr.GetSpaceObject(projHandle);
      obj->Velocity = { 0.0f, 0.0f, 100.0f }; // Moving along Z

      // Tick movement for 1 second in small steps.
      for (int i = 0; i < 20; ++i)
        movement.Update(0.05f);

      // Should have moved ~100 units along Z.
      float z = obj->Position.z;
      Assert::IsTrue(z > 90.0f && z < 110.0f, L"Projectile should travel ~100 units in 1s");
    }

    TEST_METHOD(DespawnRemovesProjectiles)
    {
      SpaceObjectManager mgr;
      ProjectileSystem projectiles(mgr);

      auto proj1 = mgr.CreateEntity(SpaceObjectCategory::Projectile, { 0, 0, 0 }, 0, 0);
      auto proj2 = mgr.CreateEntity(SpaceObjectCategory::Projectile, { 10, 0, 0 }, 0, 0);
      mgr.GetProjectileData(proj1)->Lifetime = 5.0f;
      mgr.GetProjectileData(proj2)->Lifetime = 5.0f;

      Assert::AreEqual(uint32_t(2), mgr.ActiveCount());

      // Despawn proj1 as a "hit".
      std::vector<EntityHandle> hits = { proj1 };
      projectiles.DespawnProjectiles(hits);

      Assert::AreEqual(uint32_t(1), mgr.ActiveCount());
      Assert::IsFalse(mgr.IsValid(proj1));
      Assert::IsTrue(mgr.IsValid(proj2));
    }
  };

  // =======================================================================
  // LootSystem Tests
  // =======================================================================
  TEST_CLASS(LootSystemTests)
  {
  public:
    TEST_METHOD(ShipCrateCollisionGeneratesLootEvent)
    {
      SpaceObjectManager mgr;
      LootSystem loot(mgr);

      auto shipHandle = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0, 0, 0 }, 0, 0);
      auto crateHandle = mgr.CreateEntity(SpaceObjectCategory::Crate, { 0, 0, 0 }, 0, 0);
      CrateData* crate = mgr.GetCrateData(crateHandle);
      crate->LootTableId = 42;

      std::vector<CollisionEvent> collisions = { { shipHandle, crateHandle } };
      loot.ProcessCollisions(collisions);

      Assert::AreEqual(size_t(1), loot.GetLootEvents().size());
      Assert::IsTrue(loot.GetLootEvents()[0].Ship == shipHandle);
      Assert::AreEqual(uint32_t(42), loot.GetLootEvents()[0].LootTableId);

      // Crate should be destroyed.
      Assert::IsFalse(mgr.IsValid(crateHandle));
    }

    TEST_METHOD(NonShipCrateCollisionIgnored)
    {
      SpaceObjectManager mgr;
      LootSystem loot(mgr);

      auto asteroidHandle = mgr.CreateEntity(SpaceObjectCategory::Asteroid, { 0, 0, 0 }, 0, 0);
      auto crateHandle = mgr.CreateEntity(SpaceObjectCategory::Crate, { 0, 0, 0 }, 0, 0);

      std::vector<CollisionEvent> collisions = { { asteroidHandle, crateHandle } };
      loot.ProcessCollisions(collisions);

      Assert::AreEqual(size_t(0), loot.GetLootEvents().size());
      Assert::IsTrue(mgr.IsValid(crateHandle), L"Crate should not be destroyed");
    }
  };

  // =======================================================================
  // DockingSystem Tests
  // =======================================================================
  TEST_CLASS(DockingSystemTests)
  {
  public:
    TEST_METHOD(ShipInDockingRangeGeneratesDockEvent)
    {
      SpaceObjectManager mgr;
      DockingSystem docking(mgr);

      auto shipHandle = mgr.CreateEntity(SpaceObjectCategory::Ship, { 5, 0, 0 }, 0, 0);
      auto stationHandle = mgr.CreateEntity(SpaceObjectCategory::Station, { 0, 0, 0 }, 0, 0);
      StationData* station = mgr.GetStationData(stationHandle);
      station->DockingRadius = 10.0f;

      // Simulate collision event (broad-phase).
      std::vector<CollisionEvent> collisions = { { shipHandle, stationHandle } };
      docking.ProcessCollisions(collisions);

      Assert::AreEqual(size_t(1), docking.GetDockEvents().size());
      Assert::IsTrue(docking.GetDockEvents()[0].Ship == shipHandle);
      Assert::IsTrue(docking.GetDockEvents()[0].Station == stationHandle);
    }

    TEST_METHOD(ShipOutsideDockingRangeNoDockEvent)
    {
      SpaceObjectManager mgr;
      DockingSystem docking(mgr);

      auto shipHandle = mgr.CreateEntity(SpaceObjectCategory::Ship, { 50, 0, 0 }, 0, 0);
      auto stationHandle = mgr.CreateEntity(SpaceObjectCategory::Station, { 0, 0, 0 }, 0, 0);
      StationData* station = mgr.GetStationData(stationHandle);
      station->DockingRadius = 10.0f;

      // Even if broad-phase detects them, docking range check rejects.
      std::vector<CollisionEvent> collisions = { { shipHandle, stationHandle } };
      docking.ProcessCollisions(collisions);

      Assert::AreEqual(size_t(0), docking.GetDockEvents().size());
    }

    TEST_METHOD(DirectDockingRangeCheck)
    {
      SpaceObjectManager mgr;
      DockingSystem docking(mgr);

      auto shipHandle = mgr.CreateEntity(SpaceObjectCategory::Ship, { 3, 0, 0 }, 0, 0);
      auto stationHandle = mgr.CreateEntity(SpaceObjectCategory::Station, { 0, 0, 0 }, 0, 0);
      StationData* station = mgr.GetStationData(stationHandle);
      station->DockingRadius = 5.0f;

      docking.CheckDockingRange(shipHandle, stationHandle);

      Assert::AreEqual(size_t(1), docking.GetDockEvents().size());
    }
  };

  // =======================================================================
  // NPC AI Tests
  // =======================================================================
  TEST_CLASS(NpcAITests)
  {
  public:
    TEST_METHOD(NpcStartsInPatrolState)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      CombatSystem combat(mgr, movement);
      NpcAI ai(mgr, movement, combat);

      auto npcHandle = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0, 0, 0 }, 0, 0);
      ShipData* ship = mgr.GetShipData(npcHandle);
      ship->MaxSpeed = 50.0f;
      ship->TurnRate = 2.0f;
      ship->Thrust = 20.0f;
      ship->FactionId = 1;

      XMFLOAT3 waypoints[] = { { 10, 0, 0 }, { -10, 0, 0 } };
      ai.RegisterNpc(npcHandle, 1, waypoints, 2);

      const NpcState* state = ai.GetNpcState(npcHandle);
      Assert::IsNotNull(state);
      Assert::IsTrue(state->State == AiState::Patrol);
    }

    TEST_METHOD(NpcTransitionsToAggroOnHostile)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      CombatSystem combat(mgr, movement);
      NpcAI ai(mgr, movement, combat);

      auto npcHandle = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0, 0, 0 }, 0, 0);
      ShipData* npcShip = mgr.GetShipData(npcHandle);
      npcShip->MaxSpeed = 50.0f;
      npcShip->TurnRate = 2.0f;
      npcShip->Thrust = 20.0f;
      npcShip->FactionId = 1;

      XMFLOAT3 waypoints[] = { { 100, 0, 0 } };
      ai.RegisterNpc(npcHandle, 1, waypoints, 1, 50.0f); // 50 unit aggro radius

      // Place an enemy faction ship nearby.
      auto enemyHandle = mgr.CreateEntity(SpaceObjectCategory::Ship, { 10, 0, 0 }, 0, 0);
      ShipData* enemyShip = mgr.GetShipData(enemyHandle);
      enemyShip->FactionId = 2; // Different faction

      // Tick AI — scan timer starts at 0, so first tick will scan.
      ai.Update(0.05f);

      const NpcState* state = ai.GetNpcState(npcHandle);
      // Should detect hostile and transition to Aggro or Chase.
      Assert::IsTrue(state->State == AiState::Aggro || state->State == AiState::Chase,
        L"NPC should detect hostile and aggro/chase");
    }

    TEST_METHOD(NpcTransitionsFromAggroToChase)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      CombatSystem combat(mgr, movement);
      NpcAI ai(mgr, movement, combat);

      auto npcHandle = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0, 0, 0 }, 0, 0);
      ShipData* npcShip = mgr.GetShipData(npcHandle);
      npcShip->MaxSpeed = 50.0f;
      npcShip->TurnRate = 2.0f;
      npcShip->Thrust = 20.0f;
      npcShip->FactionId = 1;

      XMFLOAT3 waypoints[] = { { 100, 0, 0 } };
      ai.RegisterNpc(npcHandle, 1, waypoints, 1, 50.0f, 20.0f);

      auto enemyHandle = mgr.CreateEntity(SpaceObjectCategory::Ship, { 30, 0, 0 }, 0, 0);
      mgr.GetShipData(enemyHandle)->FactionId = 2;

      // First tick: scan → aggro.
      ai.Update(0.05f);
      // Second tick: aggro → chase.
      ai.Update(0.05f);

      const NpcState* state = ai.GetNpcState(npcHandle);
      Assert::IsTrue(state->State == AiState::Chase, L"NPC should be in Chase state");
    }

    TEST_METHOD(NpcTransitionsToAttackInRange)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      CombatSystem combat(mgr, movement);
      NpcAI ai(mgr, movement, combat);

      auto npcHandle = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0, 0, 0 }, 0, 0);
      ShipData* npcShip = mgr.GetShipData(npcHandle);
      npcShip->MaxSpeed = 50.0f;
      npcShip->TurnRate = 2.0f;
      npcShip->Thrust = 20.0f;
      npcShip->FactionId = 1;

      XMFLOAT3 waypoints[] = { { 100, 0, 0 } };
      ai.RegisterNpc(npcHandle, 1, waypoints, 1, 50.0f, 30.0f); // 30 unit attack range

      // Place enemy within attack range.
      auto enemyHandle = mgr.CreateEntity(SpaceObjectCategory::Ship, { 5, 0, 0 }, 0, 0);
      mgr.GetShipData(enemyHandle)->FactionId = 2;

      // Tick: scan → aggro → chase → attack (enemy is within attack range).
      ai.Update(0.05f); // Aggro
      ai.Update(0.05f); // Chase → checks distance → within attack range → Attack
      ai.Update(0.05f); // Attack state sustained

      const NpcState* state = ai.GetNpcState(npcHandle);
      Assert::IsTrue(state->State == AiState::Attack, L"NPC should be in Attack state");
    }

    TEST_METHOD(NpcReturnsWhenTargetDestroyed)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      CombatSystem combat(mgr, movement);
      NpcAI ai(mgr, movement, combat);

      auto npcHandle = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0, 0, 0 }, 0, 0);
      ShipData* npcShip = mgr.GetShipData(npcHandle);
      npcShip->MaxSpeed = 50.0f;
      npcShip->TurnRate = 2.0f;
      npcShip->Thrust = 20.0f;
      npcShip->FactionId = 1;

      XMFLOAT3 waypoints[] = { { 100, 0, 0 } };
      ai.RegisterNpc(npcHandle, 1, waypoints, 1, 50.0f, 30.0f);

      auto enemyHandle = mgr.CreateEntity(SpaceObjectCategory::Ship, { 5, 0, 0 }, 0, 0);
      mgr.GetShipData(enemyHandle)->FactionId = 2;

      // Get to Attack state.
      ai.Update(0.05f);
      ai.Update(0.05f);
      ai.Update(0.05f);

      // Destroy the enemy.
      mgr.DestroyEntity(enemyHandle);

      // Next tick: target gone → return to patrol.
      ai.Update(0.05f);

      const NpcState* state = ai.GetNpcState(npcHandle);
      Assert::IsTrue(state->State == AiState::Return, L"NPC should return when target destroyed");
    }

    TEST_METHOD(IdleNpcWithNoWaypointsScansPeriodically)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      CombatSystem combat(mgr, movement);
      NpcAI ai(mgr, movement, combat);

      auto npcHandle = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0, 0, 0 }, 0, 0);
      ShipData* npcShip = mgr.GetShipData(npcHandle);
      npcShip->FactionId = 1;

      ai.RegisterNpc(npcHandle, 1, nullptr, 0); // No waypoints → Idle

      const NpcState* state = ai.GetNpcState(npcHandle);
      Assert::IsTrue(state->State == AiState::Idle);

      // Tick without hostiles — should remain Idle.
      ai.Update(2.0f); // Past scan interval
      Assert::IsTrue(ai.GetNpcState(npcHandle)->State == AiState::Idle);
    }
  };

  // =======================================================================
  // FleetSystem Tests
  // =======================================================================
  TEST_CLASS(FleetSystemTests)
  {
  public:
    TEST_METHOD(CreateFleetAndAddShips)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      FleetSystem fleets(mgr, movement);

      auto& fleet = fleets.CreateFleet(100);
      Assert::AreEqual(uint32_t(100), fleet.SessionId);
      Assert::AreEqual(uint8_t(0), fleet.ShipCount);

      auto ship1 = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0, 0, 0 }, 0, 0);
      auto ship2 = mgr.CreateEntity(SpaceObjectCategory::Ship, { 10, 0, 0 }, 0, 0);

      Assert::IsTrue(fleets.AddShipToFleet(100, ship1));
      Assert::IsTrue(fleets.AddShipToFleet(100, ship2));

      auto* f = fleets.GetFleet(100);
      Assert::IsNotNull(f);
      Assert::AreEqual(uint8_t(2), f->ShipCount);
    }

    TEST_METHOD(RemoveShipFromFleet)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      FleetSystem fleets(mgr, movement);

      fleets.CreateFleet(100);
      auto ship1 = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0, 0, 0 }, 0, 0);
      auto ship2 = mgr.CreateEntity(SpaceObjectCategory::Ship, { 10, 0, 0 }, 0, 0);
      fleets.AddShipToFleet(100, ship1);
      fleets.AddShipToFleet(100, ship2);

      Assert::IsTrue(fleets.RemoveShipFromFleet(100, ship1));
      Assert::AreEqual(uint8_t(1), fleets.GetFleet(100)->ShipCount);
    }

    TEST_METHOD(RemoveShipFromAllFleets)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      FleetSystem fleets(mgr, movement);

      fleets.CreateFleet(100);
      fleets.CreateFleet(200);
      auto ship = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0, 0, 0 }, 0, 0);
      fleets.AddShipToFleet(100, ship);
      fleets.AddShipToFleet(200, ship);

      fleets.RemoveShipFromAllFleets(ship);

      Assert::AreEqual(uint8_t(0), fleets.GetFleet(100)->ShipCount);
      Assert::AreEqual(uint8_t(0), fleets.GetFleet(200)->ShipCount);
    }

    TEST_METHOD(FormationMoveToSetsTargets)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      FleetSystem fleets(mgr, movement);

      fleets.CreateFleet(100);
      auto ship1 = mgr.CreateEntity(SpaceObjectCategory::Ship, { 0, 0, 0 }, 0, 0);
      auto ship2 = mgr.CreateEntity(SpaceObjectCategory::Ship, { 10, 0, 0 }, 0, 0);
      mgr.GetShipData(ship1)->MaxSpeed = 50.0f;
      mgr.GetShipData(ship2)->MaxSpeed = 50.0f;
      fleets.AddShipToFleet(100, ship1);
      fleets.AddShipToFleet(100, ship2);

      fleets.FormationMoveTo(100, { 50, 0, 50 });

      Assert::IsTrue(movement.HasMoveTarget(ship1), L"Ship 1 should have move target");
      Assert::IsTrue(movement.HasMoveTarget(ship2), L"Ship 2 should have move target");
    }

    TEST_METHOD(SetFormationType)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      FleetSystem fleets(mgr, movement);

      fleets.CreateFleet(100);
      fleets.SetFormation(100, FormationType::Wedge);

      Assert::IsTrue(fleets.GetFleet(100)->Formation == FormationType::Wedge);
    }

    TEST_METHOD(RemoveFleetCleansUp)
    {
      SpaceObjectManager mgr;
      MovementSystem movement(mgr);
      FleetSystem fleets(mgr, movement);

      fleets.CreateFleet(100);
      fleets.RemoveFleet(100);

      Assert::IsNull(fleets.GetFleet(100));
    }
  };

  // =======================================================================
  // Fleet Data Type Tests
  // =======================================================================
  TEST_CLASS(FleetDataTests)
  {
  public:
    TEST_METHOD(FleetAddRemoveShip)
    {
      Fleet fleet;
      fleet.SessionId = 1;

      EntityHandle h1(1, 0);
      EntityHandle h2(2, 0);
      EntityHandle h3(3, 0);

      Assert::IsTrue(fleet.AddShip(h1));
      Assert::IsTrue(fleet.AddShip(h2));
      Assert::AreEqual(uint8_t(2), fleet.ShipCount);

      Assert::IsTrue(fleet.ContainsShip(h1));
      Assert::IsTrue(fleet.ContainsShip(h2));
      Assert::IsFalse(fleet.ContainsShip(h3));

      Assert::IsTrue(fleet.RemoveShip(h1));
      Assert::AreEqual(uint8_t(1), fleet.ShipCount);
      Assert::IsFalse(fleet.ContainsShip(h1));
    }

    TEST_METHOD(FleetCapacityEnforced)
    {
      Fleet fleet;
      for (uint32_t i = 1; i <= MAX_FLEET_SHIPS; ++i)
        Assert::IsTrue(fleet.AddShip(EntityHandle(i, 0)));

      // Fleet is full.
      Assert::IsFalse(fleet.AddShip(EntityHandle(MAX_FLEET_SHIPS + 1, 0)));
      Assert::AreEqual(static_cast<uint8_t>(MAX_FLEET_SHIPS), fleet.ShipCount);
    }

    TEST_METHOD(FormationOffsetLeaderAtCenter)
    {
      XMFLOAT3 offset = Fleet::GetFormationOffset(FormationType::Line, 0, 5);
      Assert::AreEqual(0.0f, offset.x, 0.01f);
      Assert::AreEqual(0.0f, offset.y, 0.01f);
      Assert::AreEqual(0.0f, offset.z, 0.01f);
    }

    TEST_METHOD(FormationOffsetLineSpacing)
    {
      XMFLOAT3 offset1 = Fleet::GetFormationOffset(FormationType::Line, 1, 3);
      XMFLOAT3 offset2 = Fleet::GetFormationOffset(FormationType::Line, 2, 3);

      // Line formation: each ship behind the previous along Z.
      Assert::IsTrue(offset1.z < 0.0f, L"Ship 1 should be behind leader");
      Assert::IsTrue(offset2.z < offset1.z, L"Ship 2 should be further behind");
    }

    TEST_METHOD(FormationOffsetWedgeSymmetric)
    {
      XMFLOAT3 left = Fleet::GetFormationOffset(FormationType::Wedge, 1, 5);
      XMFLOAT3 right = Fleet::GetFormationOffset(FormationType::Wedge, 2, 5);

      // Wedge: alternating sides, both behind leader.
      Assert::IsTrue(left.z < 0.0f);
      Assert::IsTrue(right.z < 0.0f);
      // One side positive X, other negative (or vice versa).
      Assert::IsTrue(left.x * right.x < 0.0f, L"Ships should be on opposite sides");
    }
  };

  // =======================================================================
  // Integration: Combat Engagement
  // =======================================================================
  TEST_CLASS(CombatIntegrationTests)
  {
  public:
    TEST_METHOD(TwoFleetsEngageCombat)
    {
      SpaceObjectManager mgr;
      SpatialHash spatialHash;
      MovementSystem movement(mgr);
      CollisionSystem collision(mgr, spatialHash);
      CombatSystem combat(mgr, movement);
      ProjectileSystem projectiles(mgr);

      // Fleet A: one armed ship.
      auto shipA = CreateArmedShip(mgr, { 0, 0, 0 }, 0, 0, 100);

      // Fleet B: one armed ship nearby.
      auto shipB = CreateArmedShip(mgr, { 10, 0, 0 }, 0, 0, 100);

      // Set mutual attack targets.
      combat.SetAttackTarget(shipA, shipB);
      combat.SetAttackTarget(shipB, shipA);

      float dt = 0.05f;
      bool anyDamage = false;

      // Simulate 200 ticks (10 seconds).
      for (int i = 0; i < 200; ++i)
      {
        combat.Update(dt);
        movement.Update(dt);
        projectiles.Update(dt);
        collision.RebuildSpatialHash();
        collision.DetectCollisions();
        combat.ProcessCollisions(collision.GetCollisions());
        projectiles.DespawnProjectiles(combat.GetProjectileHits());

        // Check if any ship has taken damage.
        ShipData* shipDataA = mgr.GetShipData(shipA);
        ShipData* shipDataB = mgr.GetShipData(shipB);
        if (shipDataA && shipDataA->HullHP < 100.0f)
          anyDamage = true;
        if (shipDataB && shipDataB->HullHP < 100.0f)
          anyDamage = true;
      }

      Assert::IsTrue(anyDamage, L"At least one ship should take damage during engagement");
    }

    TEST_METHOD(AllSystemsTickWithoutCrash)
    {
      SpaceObjectManager mgr;
      SpatialHash spatialHash;
      MovementSystem movement(mgr);
      CollisionSystem collision(mgr, spatialHash);
      CombatSystem combat(mgr, movement);
      ProjectileSystem projectiles(mgr);
      LootSystem loot(mgr);
      DockingSystem docking(mgr);
      NpcAI npcAI(mgr, movement, combat);
      FleetSystem fleets(mgr, movement);

      // Create a small world.
      CreateArmedShip(mgr, { 0, 0, 0 }, 100, 100, 100, 1);
      CreateArmedShip(mgr, { 20, 0, 0 }, 100, 100, 100, 2);
      (void)mgr.CreateEntity(SpaceObjectCategory::Asteroid, { 50, 0, 0 }, 0, 0);
      (void)mgr.CreateEntity(SpaceObjectCategory::Station, { -50, 0, 0 }, 0, 0);
      (void)mgr.CreateEntity(SpaceObjectCategory::Crate, { 30, 0, 0 }, 0, 0);

      float dt = 0.05f;

      // Tick all systems 100 times.
      for (int i = 0; i < 100; ++i)
      {
        npcAI.Update(dt);
        combat.Update(dt);
        movement.Update(dt);
        projectiles.Update(dt);
        collision.RebuildSpatialHash();
        collision.DetectCollisions();
        const auto& collisions = collision.GetCollisions();
        combat.ProcessCollisions(collisions);
        loot.ProcessCollisions(collisions);
        docking.ProcessCollisions(collisions);
        projectiles.DespawnProjectiles(combat.GetProjectileHits());
      }

      // Should not crash — just verify we reach this point.
      Assert::IsTrue(true, L"All systems ticked 100 frames without crash");
    }
  };
}
