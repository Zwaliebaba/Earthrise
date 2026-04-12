#pragma once

#include "Event.h"
#include "GameTypes/EntityHandle.h"

// Game-level events published via EventManager.
// Server-side systems generate these; client receives analogous
// network messages and re-publishes locally for audio/effects.

namespace Neuron
{
  // An entity was destroyed (shield/hull depleted).
  struct ExplosionEvent : public Event
  {
    EntityHandle Entity;
    XMFLOAT3     Position{};
    float        Radius = 1.0f;   // Visual explosion radius
  };

  // A projectile struck a target.
  struct ProjectileHitEvent : public Event
  {
    EntityHandle Projectile;
    EntityHandle Target;
    XMFLOAT3     HitPosition{};
    float        Damage = 0.0f;
  };

  // A shield absorbed damage (visual spark / audio ping).
  struct ShieldHitEvent : public Event
  {
    EntityHandle Target;
    XMFLOAT3     HitPosition{};
    float        Damage = 0.0f;
  };

  // An entity entered or exited a docking state.
  struct DockEvent : public Event
  {
    EntityHandle Ship;
    EntityHandle Station;
    bool         Docked = true;   // true = docked, false = undocked
  };

  // Engine state changed (thrust on/off) — for engine exhaust particles.
  struct EngineEvent : public Event
  {
    EntityHandle Ship;
    XMFLOAT3     Position{};
    XMFLOAT3     ThrustDirection{};
    bool         Active = true;
  };
}
