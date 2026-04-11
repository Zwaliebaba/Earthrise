#include "pch.h"
#include "Zone.h"

namespace EarthRise
{
  Zone::Zone()
    : m_movement(m_entityManager)
    , m_collision(m_entityManager, m_spatialHash)
    , m_commands(m_entityManager, m_movement)
    , m_jumpgates(m_entityManager)
    , m_combat(m_entityManager, m_movement)
    , m_projectiles(m_entityManager)
    , m_loot(m_entityManager)
    , m_docking(m_entityManager)
    , m_npcAI(m_entityManager, m_movement, m_combat)
    , m_fleets(m_entityManager, m_movement)
  {
    // Wire up optional system references for CommandProcessor dispatch.
    m_commands.SetCombatSystem(&m_combat);
    m_commands.SetDockingSystem(&m_docking);
  }

  void Zone::Tick(float _deltaTime)
  {
    // 1. Process player commands
    m_commands.ProcessCommands();

    // 2. Run NPC AI (generates move/attack commands)
    m_npcAI.Update(_deltaTime);

    // 3. Run combat system (turret auto-fire, projectile spawning)
    m_combat.Update(_deltaTime);

    // 4. Run movement system (steering + velocity integration)
    m_movement.Update(_deltaTime);

    // 5. Manage projectile lifetime
    m_projectiles.Update(_deltaTime);

    // 6. Rebuild spatial hash from current positions
    m_collision.RebuildSpatialHash();

    // 7. Detect collisions
    m_collision.DetectCollisions();

    // 8. Process combat collisions (projectile-ship hits, damage, destruction)
    const auto& collisions = m_collision.GetCollisions();
    m_combat.ProcessCollisions(collisions);

    // 9. Despawn hit + expired projectiles
    m_projectiles.DespawnProjectiles(m_combat.GetProjectileHits());

    // 10. Process loot collisions (ship-crate)
    m_loot.ProcessCollisions(collisions);

    // 11. Process docking collisions (ship-station)
    m_docking.ProcessCollisions(collisions);

    // 12. Handle destroyed ships — remove from fleets, spawn crates
    for (const auto& destroyed : m_combat.GetDestroyedShips())
    {
      m_npcAI.UnregisterNpc(destroyed.Destroyed);
      m_fleets.RemoveShipFromAllFleets(destroyed.Destroyed);
      m_combat.ClearAttackTarget(destroyed.Destroyed);
      m_movement.ClearMoveTarget(destroyed.Destroyed);
      m_entityManager.DestroyEntity(destroyed.Destroyed);

      // Spawn a loot crate at the destruction position.
      auto crateHandle = m_entityManager.CreateEntity(
        Neuron::SpaceObjectCategory::Crate, destroyed.Position, 0, 0);
      if (crateHandle.IsValid())
      {
        Neuron::CrateData* crate = m_entityManager.GetCrateData(crateHandle);
        if (crate)
        {
          crate->LootTableId = 1; // Default loot table
          crate->Lifetime = 120.0f; // 2 minutes
        }
      }
    }

    // 13. Process jumpgate warps
    m_jumpgates.Update();

    ++m_tickCount;
  }
}
