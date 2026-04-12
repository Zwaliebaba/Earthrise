#include "pch.h"
#include "SoundBank.h"
#include "ParticleSystem.h"
#include "GameEvents.h"
#include "AudioEventHandler.h"
#include "SpatialAudioSystem.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Neuron;
using namespace Neuron::Graphics;

// ═══════════════════════════════════════════════════════════════════════════
// SoundBank tests
// ═══════════════════════════════════════════════════════════════════════════
TEST_CLASS(SoundBankTests)
{
public:
  TEST_METHOD(DefaultCatalog_HasAllSounds)
  {
    SoundBank bank;
    Assert::AreEqual(static_cast<size_t>(SoundId::COUNT), bank.Count());
  }

  TEST_METHOD(DefaultCatalog_ContainsExplosion)
  {
    SoundBank bank;
    Assert::IsTrue(bank.Contains(SoundId::Explosion));
    const auto* entry = bank.GetEntry(SoundId::Explosion);
    Assert::IsNotNull(entry);
    Assert::AreEqual(std::string("explosion.wav"), entry->FilePath);
  }

  TEST_METHOD(DefaultCatalog_ContainsEngineHum)
  {
    SoundBank bank;
    const auto* entry = bank.GetEntry(SoundId::EngineHum);
    Assert::IsNotNull(entry);
    Assert::IsTrue(entry->Looping);
  }

  TEST_METHOD(DefaultCatalog_AmbientSpaceIsLooping)
  {
    SoundBank bank;
    const auto* entry = bank.GetEntry(SoundId::AmbientSpace);
    Assert::IsNotNull(entry);
    Assert::IsTrue(entry->Looping);
  }

  TEST_METHOD(DefaultCatalog_WeaponFireNotLooping)
  {
    SoundBank bank;
    const auto* entry = bank.GetEntry(SoundId::WeaponFire);
    Assert::IsNotNull(entry);
    Assert::IsFalse(entry->Looping);
  }

  TEST_METHOD(Register_OverwritesExisting)
  {
    SoundBank bank;
    SoundEntry custom;
    custom.Id = SoundId::Explosion;
    custom.FilePath = "big_boom.wav";
    custom.BaseVolume = 0.9f;
    bank.Register(custom);

    const auto* entry = bank.GetEntry(SoundId::Explosion);
    Assert::IsNotNull(entry);
    Assert::AreEqual(std::string("big_boom.wav"), entry->FilePath);
    Assert::AreEqual(0.9f, entry->BaseVolume, 0.001f);
  }

  TEST_METHOD(GetEntry_Invalid_ReturnsNull)
  {
    SoundBank bank;
    const auto* entry = bank.GetEntry(SoundId::COUNT);
    Assert::IsNull(entry);
  }

  TEST_METHOD(GetAll_ReturnsCorrectCount)
  {
    SoundBank bank;
    Assert::AreEqual(bank.Count(), bank.GetAll().size());
  }

  TEST_METHOD(DefaultCatalog_DistancesValid)
  {
    SoundBank bank;
    for (const auto& [id, entry] : bank.GetAll())
    {
      Assert::IsTrue(entry.MaxDistance >= entry.MinDistance,
        L"MaxDistance must be >= MinDistance");
      Assert::IsTrue(entry.BaseVolume >= 0.0f && entry.BaseVolume <= 1.0f,
        L"BaseVolume must be in [0,1]");
    }
  }

  TEST_METHOD(DefaultCatalog_AllSoundIdsUnique)
  {
    SoundBank bank;
    std::set<SoundId> seen;
    for (const auto& [id, entry] : bank.GetAll())
    {
      Assert::IsTrue(seen.insert(id).second, L"Duplicate SoundId");
    }
  }
};

// ═══════════════════════════════════════════════════════════════════════════
// ParticleSystem tests
// ═══════════════════════════════════════════════════════════════════════════
TEST_CLASS(ParticleSystemTests)
{
public:
  TEST_METHOD(InitiallyEmpty)
  {
    ParticleSystem ps;
    Assert::AreEqual(0u, ps.ActiveCount());
  }

  TEST_METHOD(SpawnBurst_CreatesParticles)
  {
    ParticleSystem ps;
    ps.SpawnBurst({ 0, 0, 0 }, 1.0f, { 1, 0, 0, 1 }, 10);
    Assert::AreEqual(10u, ps.ActiveCount());
  }

  TEST_METHOD(SpawnSingle_AddsOne)
  {
    ParticleSystem ps;
    Particle p;
    p.Position = { 1, 2, 3 };
    p.Lifetime = 5.0f;
    ps.SpawnParticle(p);
    Assert::AreEqual(1u, ps.ActiveCount());
  }

  TEST_METHOD(Update_IntegratesPosition)
  {
    ParticleSystem ps;
    Particle p;
    p.Position = { 0, 0, 0 };
    p.Velocity = { 10, 0, 0 };
    p.Lifetime = 5.0f;
    ps.SpawnParticle(p);

    ps.Update(1.0f);

    const auto& particles = ps.GetParticles();
    Assert::AreEqual(1u, ps.ActiveCount());
    Assert::AreEqual(10.0f, particles[0].Position.x, 0.01f);
  }

  TEST_METHOD(Update_AgesParticles)
  {
    ParticleSystem ps;
    Particle p;
    p.Lifetime = 2.0f;
    ps.SpawnParticle(p);

    ps.Update(0.5f);
    Assert::AreEqual(0.5f, ps.GetParticles()[0].Age, 0.001f);
  }

  TEST_METHOD(Update_RemovesExpiredParticles)
  {
    ParticleSystem ps;
    Particle p;
    p.Lifetime = 1.0f;
    ps.SpawnParticle(p);

    ps.Update(1.5f); // Beyond lifetime
    Assert::AreEqual(0u, ps.ActiveCount());
  }

  TEST_METHOD(Update_FadesAlpha)
  {
    ParticleSystem ps;
    Particle p;
    p.Lifetime = 2.0f;
    p.Color = { 1, 1, 1, 1 };
    ps.SpawnParticle(p);

    ps.Update(1.0f); // Half lifetime
    // Alpha should be approximately 0.5
    Assert::AreEqual(0.5f, ps.GetParticles()[0].Color.w, 0.01f);
  }

  TEST_METHOD(SpawnBurst_RespectsMaxParticles)
  {
    ParticleSystem ps;
    // Try to spawn more than MAX_PARTICLES
    ps.SpawnBurst({ 0, 0, 0 }, 1.0f, { 1, 1, 1, 1 }, ParticleSystem::MAX_PARTICLES + 100);
    Assert::AreEqual(ParticleSystem::MAX_PARTICLES, ps.ActiveCount());
  }

  TEST_METHOD(SpawnSingle_RespectsMaxParticles)
  {
    ParticleSystem ps;
    // Fill to max
    ps.SpawnBurst({ 0, 0, 0 }, 1.0f, { 1, 1, 1, 1 }, ParticleSystem::MAX_PARTICLES);
    Assert::AreEqual(ParticleSystem::MAX_PARTICLES, ps.ActiveCount());

    // Try to add one more
    Particle p;
    ps.SpawnParticle(p);
    Assert::AreEqual(ParticleSystem::MAX_PARTICLES, ps.ActiveCount());
  }

  TEST_METHOD(Clear_RemovesAll)
  {
    ParticleSystem ps;
    ps.SpawnBurst({ 0, 0, 0 }, 1.0f, { 1, 1, 1, 1 }, 50);
    Assert::AreEqual(50u, ps.ActiveCount());

    ps.Clear();
    Assert::AreEqual(0u, ps.ActiveCount());
  }

  TEST_METHOD(SpawnBurst_VelocitiesAreNonZero)
  {
    ParticleSystem ps;
    ps.SpawnBurst({ 0, 0, 0 }, 2.0f, { 1, 1, 1, 1 }, 20);

    bool anyNonZero = false;
    for (const auto& p : ps.GetParticles())
    {
      if (p.Velocity.x != 0 || p.Velocity.y != 0 || p.Velocity.z != 0)
      {
        anyNonZero = true;
        break;
      }
    }
    Assert::IsTrue(anyNonZero, L"Burst particles should have random velocities");
  }

  TEST_METHOD(SpawnBurst_LifetimesVary)
  {
    ParticleSystem ps;
    ps.SpawnBurst({ 0, 0, 0 }, 1.0f, { 1, 1, 1, 1 }, 30);

    float minLife = FLT_MAX, maxLife = 0.0f;
    for (const auto& p : ps.GetParticles())
    {
      minLife = std::min(minLife, p.Lifetime);
      maxLife = std::max(maxLife, p.Lifetime);
    }
    Assert::IsTrue(maxLife > minLife, L"Lifetimes should vary randomly");
  }

  TEST_METHOD(MultipleUpdates_GradualRemoval)
  {
    ParticleSystem ps;
    // Spawn with varying lifetimes via burst
    ps.SpawnBurst({ 0, 0, 0 }, 1.0f, { 1, 1, 1, 1 }, 20);
    uint32_t initial = ps.ActiveCount();

    // Step time forward in increments
    ps.Update(0.5f);
    ps.Update(0.5f);
    ps.Update(0.5f);

    // Some should have died by now (min lifetime is 0.3)
    Assert::IsTrue(ps.ActiveCount() < initial, L"Particles should expire over time");
  }
};

// ═══════════════════════════════════════════════════════════════════════════
// GameEvents tests
// ═══════════════════════════════════════════════════════════════════════════
TEST_CLASS(GameEventsTests)
{
public:
  TEST_METHOD(ExplosionEvent_PublishAndSubscribe)
  {
    struct Listener : public EventSubscriber
    {
      int count = 0;
      XMFLOAT3 lastPos{};
      void OnExplosion(ExplosionEvent& e)
      {
        ++count;
        lastPos = e.Position;
      }
    };

    Listener listener;
    EventManager::Subscribe<ExplosionEvent>(&listener, &Listener::OnExplosion);

    ExplosionEvent evt;
    evt.Position = { 10, 20, 30 };
    evt.Radius = 5.0f;
    EventManager::Publish(evt);

    Assert::AreEqual(1, listener.count);
    Assert::AreEqual(10.0f, listener.lastPos.x, 0.001f);
    Assert::AreEqual(20.0f, listener.lastPos.y, 0.001f);
    Assert::AreEqual(30.0f, listener.lastPos.z, 0.001f);
  }

  TEST_METHOD(ProjectileHitEvent_PublishAndSubscribe)
  {
    struct Listener : public EventSubscriber
    {
      int count = 0;
      void OnHit(ProjectileHitEvent& e) { ++count; }
    };

    Listener listener;
    EventManager::Subscribe<ProjectileHitEvent>(&listener, &Listener::OnHit);

    ProjectileHitEvent evt;
    evt.HitPosition = { 1, 2, 3 };
    evt.Damage = 25.0f;
    EventManager::Publish(evt);

    Assert::AreEqual(1, listener.count);
  }

  TEST_METHOD(ShieldHitEvent_PublishAndSubscribe)
  {
    struct Listener : public EventSubscriber
    {
      int count = 0;
      void OnShield(ShieldHitEvent& e) { ++count; }
    };

    Listener listener;
    EventManager::Subscribe<ShieldHitEvent>(&listener, &Listener::OnShield);

    ShieldHitEvent evt;
    EventManager::Publish(evt);

    Assert::AreEqual(1, listener.count);
  }

  TEST_METHOD(DockEvent_CarriesState)
  {
    struct Listener : public EventSubscriber
    {
      bool docked = false;
      void OnDock(DockEvent& e) { docked = e.Docked; }
    };

    Listener listener;
    EventManager::Subscribe<DockEvent>(&listener, &Listener::OnDock);

    DockEvent evt;
    evt.Docked = true;
    EventManager::Publish(evt);
    Assert::IsTrue(listener.docked);

    evt.Docked = false;
    EventManager::Publish(evt);
    Assert::IsFalse(listener.docked);
  }

  TEST_METHOD(EngineEvent_CarriesActive)
  {
    struct Listener : public EventSubscriber
    {
      bool active = false;
      void OnEngine(EngineEvent& e) { active = e.Active; }
    };

    Listener listener;
    EventManager::Subscribe<EngineEvent>(&listener, &Listener::OnEngine);

    EngineEvent evt;
    evt.Active = true;
    EventManager::Publish(evt);
    Assert::IsTrue(listener.active);
  }

  TEST_METHOD(SetConsumed_StopsChain)
  {
    struct Listener1 : public EventSubscriber
    {
      int count = 0;
      void OnExplosion(ExplosionEvent& e) { ++count; e.SetConsumed(); }
    };
    struct Listener2 : public EventSubscriber
    {
      int count = 0;
      void OnExplosion(ExplosionEvent& e) { ++count; }
    };

    Listener1 l1;
    Listener2 l2;
    EventManager::Subscribe<ExplosionEvent>(&l1, &Listener1::OnExplosion);
    EventManager::Subscribe<ExplosionEvent>(&l2, &Listener2::OnExplosion);

    ExplosionEvent evt;
    EventManager::Publish(evt);

    Assert::AreEqual(1, l1.count);
    Assert::AreEqual(0, l2.count); // Should not have received it
  }
};

// ═══════════════════════════════════════════════════════════════════════════
// AudioEventHandler tests (no real audio — just verifies event routing)
// ═══════════════════════════════════════════════════════════════════════════
TEST_CLASS(AudioEventHandlerTests)
{
public:
  TEST_METHOD(Initialize_SubscribesToEvents)
  {
    AudioEventHandler handler;
    handler.Initialize(nullptr, nullptr); // No audio or particles

    ExplosionEvent evt;
    evt.Position = { 5, 5, 5 };
    evt.Radius = 3.0f;
    EventManager::Publish(evt);

    Assert::AreEqual(1u, handler.GetExplosionCount());
  }

  TEST_METHOD(OnProjectileHit_IncrementsCount)
  {
    AudioEventHandler handler;
    handler.Initialize(nullptr, nullptr);

    ProjectileHitEvent evt;
    EventManager::Publish(evt);

    Assert::AreEqual(1u, handler.GetHitCount());
  }

  TEST_METHOD(OnShieldHit_IncrementsCount)
  {
    AudioEventHandler handler;
    handler.Initialize(nullptr, nullptr);

    ShieldHitEvent evt;
    EventManager::Publish(evt);

    Assert::AreEqual(1u, handler.GetShieldHitCount());
  }

  TEST_METHOD(OnDock_IncrementsCount)
  {
    AudioEventHandler handler;
    handler.Initialize(nullptr, nullptr);

    DockEvent evt;
    evt.Docked = true;
    EventManager::Publish(evt);

    Assert::AreEqual(1u, handler.GetDockCount());
  }

  TEST_METHOD(MultipleExplosions_Accumulate)
  {
    AudioEventHandler handler;
    handler.Initialize(nullptr, nullptr);

    for (int i = 0; i < 5; ++i)
    {
      ExplosionEvent evt;
      EventManager::Publish(evt);
    }

    Assert::AreEqual(5u, handler.GetExplosionCount());
  }

  TEST_METHOD(ExplosionWithParticles_SpawnsParticles)
  {
    ParticleSystem particles;
    AudioEventHandler handler;
    handler.Initialize(nullptr, &particles);

    ExplosionEvent evt;
    evt.Position = { 10, 20, 30 };
    evt.Radius = 2.0f;
    EventManager::Publish(evt);

    Assert::IsTrue(particles.ActiveCount() > 0, L"Explosion should spawn particles");
  }

  TEST_METHOD(ShieldHitWithParticles_SpawnsParticles)
  {
    ParticleSystem particles;
    AudioEventHandler handler;
    handler.Initialize(nullptr, &particles);

    ShieldHitEvent evt;
    evt.HitPosition = { 1, 2, 3 };
    EventManager::Publish(evt);

    Assert::IsTrue(particles.ActiveCount() > 0, L"Shield hit should spawn sparks");
  }

  TEST_METHOD(ProjectileHitWithParticles_SpawnsParticles)
  {
    ParticleSystem particles;
    AudioEventHandler handler;
    handler.Initialize(nullptr, &particles);

    ProjectileHitEvent evt;
    evt.HitPosition = { 0, 0, 0 };
    EventManager::Publish(evt);

    Assert::IsTrue(particles.ActiveCount() > 0, L"Projectile hit should spawn particles");
  }

  TEST_METHOD(EngineActiveWithParticles_SpawnsParticles)
  {
    ParticleSystem particles;
    AudioEventHandler handler;
    handler.Initialize(nullptr, &particles);

    EngineEvent evt;
    evt.Active = true;
    evt.Position = { 0, 0, 0 };
    EventManager::Publish(evt);

    Assert::IsTrue(particles.ActiveCount() > 0, L"Engine active should spawn exhaust");
  }

  TEST_METHOD(EngineInactive_NoParticles)
  {
    ParticleSystem particles;
    AudioEventHandler handler;
    handler.Initialize(nullptr, &particles);

    EngineEvent evt;
    evt.Active = false;
    EventManager::Publish(evt);

    Assert::AreEqual(0u, particles.ActiveCount());
  }
};

// ═══════════════════════════════════════════════════════════════════════════
// SpatialAudioSystem tests (no audio hardware — tests logic only)
// ═══════════════════════════════════════════════════════════════════════════
TEST_CLASS(SpatialAudioSystemTests)
{
public:
  TEST_METHOD(Initialize_SilentMode)
  {
    SpatialAudioSystem sas;
    sas.Initialize(nullptr); // No engine
    Assert::AreEqual(static_cast<size_t>(0), sas.LoadedCount());
  }

  TEST_METHOD(IsLoaded_ReturnsFalse_WhenNotLoaded)
  {
    SpatialAudioSystem sas;
    sas.Initialize(nullptr);
    Assert::IsFalse(sas.IsLoaded(SoundId::Explosion));
  }

  TEST_METHOD(PlayOneShot_SilentMode_NoThrow)
  {
    SpatialAudioSystem sas;
    sas.Initialize(nullptr);
    // Should silently do nothing
    XMVECTOR pos = XMVectorSet(10, 20, 30, 0);
    sas.PlayOneShot(SoundId::Explosion, pos);
  }

  TEST_METHOD(PlayUI_SilentMode_NoThrow)
  {
    SpatialAudioSystem sas;
    sas.Initialize(nullptr);
    sas.PlayUI(SoundId::TargetLock);
  }

  TEST_METHOD(PreloadFromBank_SilentMode_LoadsNothing)
  {
    SpatialAudioSystem sas;
    sas.Initialize(nullptr);
    SoundBank bank;
    sas.PreloadFromBank(bank);
    Assert::AreEqual(static_cast<size_t>(0), sas.LoadedCount());
  }

  TEST_METHOD(UpdateListener_SilentMode_NoThrow)
  {
    SpatialAudioSystem sas;
    sas.Initialize(nullptr);
    XMVECTOR pos = XMVectorSet(0, 0, 0, 0);
    XMVECTOR fwd = XMVectorSet(0, 0, -1, 0);
    XMVECTOR up  = XMVectorSet(0, 1, 0, 0);
    sas.UpdateListener(pos, fwd, up, 0.016f);
  }
};
