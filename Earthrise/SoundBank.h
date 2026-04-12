#pragma once

// SoundBank — catalog of game sound effects.
// Maps logical sound IDs to WAV file paths.
// Actual audio loading is deferred to AudioEngine; SoundBank
// only manages the registry and lookup.

#include <string>
#include <string_view>
#include <unordered_map>
#include <array>

enum class SoundId : uint8_t
{
  EngineHum,
  WeaponFire,
  Explosion,
  ShieldHit,
  AmbientSpace,
  DockingClamp,
  UndockRelease,
  ProjectileLaunch,
  TargetLock,
  COUNT
};

struct SoundEntry
{
  SoundId     Id       = SoundId::COUNT;
  std::string FilePath;       // Relative to Assets/Audio/
  float       BaseVolume = 1.0f;
  float       MinDistance = 5.0f;   // Distance below which sound is at full volume
  float       MaxDistance = 500.0f; // Distance above which sound is inaudible
  bool        Looping    = false;
};

class SoundBank
{
public:
  SoundBank() { BuildDefaultCatalog(); }

  // Register a sound entry. Overwrites if SoundId already exists.
  void Register(const SoundEntry& _entry);

  // Look up a sound entry by ID. Returns nullptr if not found.
  [[nodiscard]] const SoundEntry* GetEntry(SoundId _id) const noexcept;

  // Get all registered entries.
  [[nodiscard]] const std::unordered_map<SoundId, SoundEntry>& GetAll() const noexcept
  {
    return m_entries;
  }

  // Number of registered sounds.
  [[nodiscard]] size_t Count() const noexcept { return m_entries.size(); }

  // Check if a specific sound is registered.
  [[nodiscard]] bool Contains(SoundId _id) const noexcept
  {
    return m_entries.contains(_id);
  }

private:
  void BuildDefaultCatalog();

  std::unordered_map<SoundId, SoundEntry> m_entries;
};
