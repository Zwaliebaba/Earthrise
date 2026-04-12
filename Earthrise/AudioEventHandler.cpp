#include "pch.h"
#include "AudioEventHandler.h"
#include "SpatialAudioSystem.h"
#include "ParticleSystem.h"

AudioEventHandler::~AudioEventHandler()
{
  // EventSubscriber base class auto-unsubscribes.
}

void AudioEventHandler::Initialize(SpatialAudioSystem* _audio, Neuron::Graphics::ParticleSystem* _particles)
{
  m_audio     = _audio;
  m_particles = _particles;

  // Subscribe to game events.
  EventManager::Subscribe<Neuron::ExplosionEvent>(this, &AudioEventHandler::OnExplosion);
  EventManager::Subscribe<Neuron::ProjectileHitEvent>(this, &AudioEventHandler::OnProjectileHit);
  EventManager::Subscribe<Neuron::ShieldHitEvent>(this, &AudioEventHandler::OnShieldHit);
  EventManager::Subscribe<Neuron::DockEvent>(this, &AudioEventHandler::OnDock);
  EventManager::Subscribe<Neuron::EngineEvent>(this, &AudioEventHandler::OnEngine);
}

void AudioEventHandler::OnExplosion(Neuron::ExplosionEvent& _event)
{
  ++m_explosionCount;

  if (m_audio)
  {
    XMVECTOR pos = XMLoadFloat3(&_event.Position);
    m_audio->PlayOneShot(SoundId::Explosion, pos);
  }

  if (m_particles)
  {
    m_particles->SpawnBurst(_event.Position, _event.Radius,
      { 1.0f, 0.6f, 0.1f, 1.0f }, 32);
  }
}

void AudioEventHandler::OnProjectileHit(Neuron::ProjectileHitEvent& _event)
{
  ++m_hitCount;

  if (m_audio)
  {
    XMVECTOR pos = XMLoadFloat3(&_event.HitPosition);
    m_audio->PlayOneShot(SoundId::WeaponFire, pos);
  }

  if (m_particles)
  {
    m_particles->SpawnBurst(_event.HitPosition, 0.5f,
      { 1.0f, 1.0f, 0.3f, 1.0f }, 8);
  }
}

void AudioEventHandler::OnShieldHit(Neuron::ShieldHitEvent& _event)
{
  ++m_shieldHitCount;

  if (m_audio)
  {
    XMVECTOR pos = XMLoadFloat3(&_event.HitPosition);
    m_audio->PlayOneShot(SoundId::ShieldHit, pos);
  }

  if (m_particles)
  {
    // Shield sparks — cyan color, small burst.
    m_particles->SpawnBurst(_event.HitPosition, 0.3f,
      { 0.2f, 0.8f, 1.0f, 1.0f }, 12);
  }
}

void AudioEventHandler::OnDock(Neuron::DockEvent& _event)
{
  ++m_dockCount;

  if (m_audio)
  {
    SoundId soundId = _event.Docked ? SoundId::DockingClamp : SoundId::UndockRelease;
    m_audio->PlayUI(soundId);
  }
}

void AudioEventHandler::OnEngine(Neuron::EngineEvent& _event)
{
  if (m_particles && _event.Active)
  {
    // Small exhaust puff at engine position.
    m_particles->SpawnBurst(_event.Position, 0.2f,
      { 0.3f, 0.5f, 1.0f, 0.6f }, 4);
  }
}
