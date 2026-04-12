#pragma once

// AudioEventHandler — subscribes to game events via EventManager
// and triggers spatial audio playback + particle spawning.

#include "EventManager.h"
#include "GameEvents.h"
#include "SoundBank.h"

class SpatialAudioSystem;

namespace Neuron::Graphics { class ParticleSystem; }

class AudioEventHandler : public EventSubscriber
{
public:
  AudioEventHandler() = default;
  ~AudioEventHandler() override;

  void Initialize(SpatialAudioSystem* _audio, Neuron::Graphics::ParticleSystem* _particles);

  // Event handlers — called by EventManager.
  void OnExplosion(Neuron::ExplosionEvent& _event);
  void OnProjectileHit(Neuron::ProjectileHitEvent& _event);
  void OnShieldHit(Neuron::ShieldHitEvent& _event);
  void OnDock(Neuron::DockEvent& _event);
  void OnEngine(Neuron::EngineEvent& _event);

  // Counters for testing/debugging.
  [[nodiscard]] uint32_t GetExplosionCount() const noexcept { return m_explosionCount; }
  [[nodiscard]] uint32_t GetHitCount() const noexcept { return m_hitCount; }
  [[nodiscard]] uint32_t GetShieldHitCount() const noexcept { return m_shieldHitCount; }
  [[nodiscard]] uint32_t GetDockCount() const noexcept { return m_dockCount; }

private:
  SpatialAudioSystem*            m_audio     = nullptr;
  Neuron::Graphics::ParticleSystem* m_particles = nullptr;

  uint32_t m_explosionCount = 0;
  uint32_t m_hitCount       = 0;
  uint32_t m_shieldHitCount = 0;
  uint32_t m_dockCount      = 0;
};
