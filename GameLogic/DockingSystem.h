#pragma once

#include "SpaceObjectManager.h"
#include "CollisionSystem.h"

namespace GameLogic
{
  // Docking event — generated when a player ship enters a station's docking radius.
  struct DockEvent
  {
    Neuron::EntityHandle Ship;
    Neuron::EntityHandle Station;
  };

  // DockingSystem — detects ship-station proximity and generates dock events.
  class DockingSystem
  {
  public:
    explicit DockingSystem(SpaceObjectManager& _manager);

    // Check each ship-station pair for docking range. Uses collision events for broad-phase.
    void ProcessCollisions(const std::vector<CollisionEvent>& _collisions);

    // Alternatively, do a direct range check for ships with a Dock command target.
    void CheckDockingRange(Neuron::EntityHandle _ship, Neuron::EntityHandle _station);

    // Get dock events from this tick.
    [[nodiscard]] const std::vector<DockEvent>& GetDockEvents() const noexcept { return m_dockEvents; }

    // Clear events for new tick.
    void ClearEvents() noexcept { m_dockEvents.clear(); }

  private:
    SpaceObjectManager& m_manager;
    std::vector<DockEvent> m_dockEvents;
  };
}
