#include "pch.h"
#include "SoundBank.h"

void SoundBank::Register(const SoundEntry& _entry)
{
  m_entries[_entry.Id] = _entry;
}

const SoundEntry* SoundBank::GetEntry(SoundId _id) const noexcept
{
  auto it = m_entries.find(_id);
  return (it != m_entries.end()) ? &it->second : nullptr;
}

void SoundBank::BuildDefaultCatalog()
{
  m_entries.clear();

  Register({ SoundId::EngineHum,        "engine_hum.wav",        0.4f,   5.0f,  200.0f, true  });
  Register({ SoundId::WeaponFire,       "weapon_fire.wav",       0.8f,  10.0f,  400.0f, false });
  Register({ SoundId::Explosion,        "explosion.wav",         1.0f,  20.0f,  800.0f, false });
  Register({ SoundId::ShieldHit,        "shield_hit.wav",        0.6f,  10.0f,  300.0f, false });
  Register({ SoundId::AmbientSpace,     "ambient_space.wav",     0.2f,   0.0f,    0.0f, true  });
  Register({ SoundId::DockingClamp,     "docking_clamp.wav",     0.7f,   5.0f,  150.0f, false });
  Register({ SoundId::UndockRelease,    "undock_release.wav",    0.7f,   5.0f,  150.0f, false });
  Register({ SoundId::ProjectileLaunch, "projectile_launch.wav", 0.5f,  10.0f,  300.0f, false });
  Register({ SoundId::TargetLock,       "target_lock.wav",       0.5f,   0.0f,    0.0f, false });
}
