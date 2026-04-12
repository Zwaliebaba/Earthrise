#pragma once

#include "SpaceObjectManager.h"
#include "SpatialHash.h"
#include "MovementSystem.h"
#include "CollisionSystem.h"
#include "CommandProcessor.h"
#include "JumpgateSystem.h"
#include "CombatSystem.h"
#include "ProjectileSystem.h"
#include "LootSystem.h"
#include "DockingSystem.h"
#include "NpcAI.h"
#include "FleetSystem.h"
#include "MiningSystem.h"
#include "RegrowthSystem.h"
#include "GameTypes/AsteroidCluster.h"

namespace EarthRise
{
  // Zone — single persistent 100 km³ simulation region.
  // Owns all server-side simulation subsystems and the entity manager.
  class Zone
  {
  public:
    Zone();

    // Simulate one tick at the given delta time.
    void Tick(float _deltaTime);

    // Access subsystems.
    [[nodiscard]] GameLogic::SpaceObjectManager& GetEntityManager() noexcept { return m_entityManager; }
    [[nodiscard]] GameLogic::SpatialHash&        GetSpatialHash() noexcept { return m_spatialHash; }
    [[nodiscard]] GameLogic::MovementSystem&     GetMovement() noexcept { return m_movement; }
    [[nodiscard]] GameLogic::CollisionSystem&    GetCollision() noexcept { return m_collision; }
    [[nodiscard]] GameLogic::CommandProcessor&   GetCommands() noexcept { return m_commands; }
    [[nodiscard]] GameLogic::JumpgateSystem&     GetJumpgates() noexcept { return m_jumpgates; }
    [[nodiscard]] GameLogic::CombatSystem&       GetCombat() noexcept { return m_combat; }
    [[nodiscard]] GameLogic::ProjectileSystem&   GetProjectiles() noexcept { return m_projectiles; }
    [[nodiscard]] GameLogic::LootSystem&         GetLoot() noexcept { return m_loot; }
    [[nodiscard]] GameLogic::DockingSystem&      GetDocking() noexcept { return m_docking; }
    [[nodiscard]] GameLogic::NpcAI&              GetNpcAI() noexcept { return m_npcAI; }
    [[nodiscard]] GameLogic::FleetSystem&        GetFleets() noexcept { return m_fleets; }
    [[nodiscard]] GameLogic::MiningSystem&       GetMining() noexcept { return m_mining; }
    [[nodiscard]] GameLogic::RegrowthSystem&     GetRegrowth() noexcept { return m_regrowth; }

    [[nodiscard]] uint32_t TickCount() const noexcept { return m_tickCount; }

    // Universe data.
    void SetUniverseSeed(uint64_t _seed) noexcept { m_universeSeed = _seed; }
    [[nodiscard]] uint64_t GetUniverseSeed() const noexcept { return m_universeSeed; }
    void SetClusters(std::vector<Neuron::AsteroidCluster>&& _clusters);
    [[nodiscard]] const std::vector<Neuron::AsteroidCluster>& GetClusters() const noexcept { return m_clusters; }
    [[nodiscard]] std::vector<Neuron::AsteroidCluster>& GetClusters() noexcept { return m_clusters; }

    // Zone dimensions.
    static constexpr float ZONE_SIZE = 100000.0f; // 100 km
    static constexpr float HALF_ZONE = ZONE_SIZE * 0.5f;

  private:
    GameLogic::SpaceObjectManager m_entityManager;
    GameLogic::SpatialHash        m_spatialHash;
    GameLogic::MovementSystem     m_movement;
    GameLogic::CollisionSystem    m_collision;
    GameLogic::CommandProcessor   m_commands;
    GameLogic::JumpgateSystem     m_jumpgates;
    GameLogic::CombatSystem       m_combat;
    GameLogic::ProjectileSystem   m_projectiles;
    GameLogic::LootSystem         m_loot;
    GameLogic::DockingSystem      m_docking;
    GameLogic::NpcAI              m_npcAI;
    GameLogic::FleetSystem        m_fleets;
    GameLogic::MiningSystem       m_mining;
    GameLogic::RegrowthSystem     m_regrowth;

    // Universe data (owned by Zone, populated by ZoneLoader::LoadUniverse).
    std::vector<Neuron::AsteroidCluster> m_clusters;
    uint64_t m_universeSeed = 0;

    uint32_t m_tickCount = 0;
  };
}
