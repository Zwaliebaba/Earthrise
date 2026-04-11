#include "pch.h"
#include "Zone.h"

namespace EarthRise
{
  Zone::Zone()
    : m_movement(m_entityManager)
    , m_collision(m_entityManager, m_spatialHash)
    , m_commands(m_entityManager, m_movement)
    , m_jumpgates(m_entityManager)
  {
  }

  void Zone::Tick(float _deltaTime)
  {
    // 1. Process player commands
    m_commands.ProcessCommands();

    // 2. Run movement system (steering + velocity integration)
    m_movement.Update(_deltaTime);

    // 3. Rebuild spatial hash from current positions
    m_collision.RebuildSpatialHash();

    // 4. Detect collisions
    m_collision.DetectCollisions();

    // 5. Process jumpgate warps
    m_jumpgates.Update();

    ++m_tickCount;
  }
}
