#pragma once

#include "SpaceObjectManager.h"
#include "MovementSystem.h"
#include "GameTypes/Fleet.h"

namespace GameLogic
{
  // FleetSystem — manages player fleet composition, formation movement, and ship acquisition.
  class FleetSystem
  {
  public:
    FleetSystem(SpaceObjectManager& _manager, MovementSystem& _movement);

    // Create a fleet for a player session. Returns a reference to the fleet.
    Neuron::Fleet& CreateFleet(uint32_t _sessionId);

    // Remove a player's fleet.
    void RemoveFleet(uint32_t _sessionId);

    // Get a player's fleet (nullptr if not found).
    [[nodiscard]] Neuron::Fleet* GetFleet(uint32_t _sessionId);
    [[nodiscard]] const Neuron::Fleet* GetFleet(uint32_t _sessionId) const;

    // Add a ship to a player's fleet.
    bool AddShipToFleet(uint32_t _sessionId, Neuron::EntityHandle _ship);

    // Remove a ship from a player's fleet (e.g., on destruction).
    bool RemoveShipFromFleet(uint32_t _sessionId, Neuron::EntityHandle _ship);

    // Remove a ship from any fleet it belongs to.
    void RemoveShipFromAllFleets(Neuron::EntityHandle _ship);

    // Issue a formation move command — each ship gets an offset position based on formation type.
    void FormationMoveTo(uint32_t _sessionId, const XMFLOAT3& _targetPosition);

    // Set formation type for a player's fleet.
    void SetFormation(uint32_t _sessionId, Neuron::FormationType _type);

    // Get all fleet handles for CommandProcessor compatibility (returns ship handles).
    [[nodiscard]] std::vector<Neuron::EntityHandle> GetFleetShips(uint32_t _sessionId) const;

  private:
    SpaceObjectManager& m_manager;
    MovementSystem&     m_movement;

    std::unordered_map<uint32_t, Neuron::Fleet> m_fleets;
  };
}
