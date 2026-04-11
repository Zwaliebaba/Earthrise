#pragma once

#include "SpaceObjectManager.h"
#include "MovementSystem.h"
#include "CombatSystem.h"

namespace GameLogic
{
  // NPC AI state machine states.
  enum class AiState : uint8_t
  {
    Idle,     // Standing still, no waypoints
    Patrol,   // Moving between waypoints
    Aggro,    // Detected hostile, transitioning to chase
    Chase,    // Pursuing target
    Attack,   // In range, firing
    Return,   // Returning to patrol route after losing target

    COUNT
  };

  // Per-NPC AI state. Stored in parallel array.
  struct NpcState
  {
    AiState  State         = AiState::Idle;
    uint16_t FactionId     = 0;

    // Patrol
    XMFLOAT3 Waypoints[4]  = {};
    uint8_t  WaypointCount = 0;
    uint8_t  CurrentWaypoint = 0;

    // Aggro / combat
    Neuron::EntityHandle Target;
    float    AggroRadius   = 40.0f;  // Detection range
    float    LeashRadius   = 80.0f;  // Give up chase range
    float    AttackRange   = 20.0f;  // Weapon range

    // Timers
    float    ScanTimer     = 0.0f;   // Time until next enemy scan
    static constexpr float SCAN_INTERVAL = 1.0f; // Scan every second
  };

  // NpcAI — simple state machine AI for NPC ships.
  // Patrol → detect enemy → chase → attack → return.
  class NpcAI
  {
  public:
    NpcAI(SpaceObjectManager& _manager, MovementSystem& _movement, CombatSystem& _combat);

    // Register an NPC ship with patrol waypoints.
    void RegisterNpc(Neuron::EntityHandle _handle, uint16_t _factionId,
                     const XMFLOAT3* _waypoints, uint8_t _waypointCount,
                     float _aggroRadius = 40.0f, float _attackRange = 20.0f);

    // Unregister an NPC (e.g., on death).
    void UnregisterNpc(Neuron::EntityHandle _handle);

    // Tick: run state machine for all registered NPCs.
    void Update(float _deltaTime);

    // Get the AI state for an NPC (for testing).
    [[nodiscard]] const NpcState* GetNpcState(Neuron::EntityHandle _handle) const;

  private:
    void UpdateIdle(Neuron::SpaceObject& _obj, NpcState& _state, float _dt);
    void UpdatePatrol(Neuron::SpaceObject& _obj, NpcState& _state, float _dt);
    void UpdateAggro(Neuron::SpaceObject& _obj, NpcState& _state, float _dt);
    void UpdateChase(Neuron::SpaceObject& _obj, NpcState& _state, float _dt);
    void UpdateAttack(Neuron::SpaceObject& _obj, NpcState& _state, float _dt);
    void UpdateReturn(Neuron::SpaceObject& _obj, NpcState& _state, float _dt);

    // Scan for nearest hostile within aggro radius.
    [[nodiscard]] Neuron::EntityHandle ScanForHostile(const Neuron::SpaceObject& _obj, const NpcState& _state);

    SpaceObjectManager& m_manager;
    MovementSystem&     m_movement;
    CombatSystem&       m_combat;

    // NPC state indexed by entity slot.
    std::vector<NpcState> m_npcStates;
    // Track which entities are NPCs.
    std::vector<bool>     m_isNpc;
  };
}
