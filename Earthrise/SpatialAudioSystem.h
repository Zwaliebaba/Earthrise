#pragma once

// SpatialAudioSystem — manages AudioListener (camera) and AudioEmitters
// for 3D-positioned game sounds. Wraps the existing AudioEngine,
// SoundEffect, and SoundEffectInstance infrastructure.

#include "SoundBank.h"
#include "GameTypes/EntityHandle.h"

namespace Neuron::Audio { class AudioEngine; class SoundEffect; class SoundEffectInstance; }

class SpatialAudioSystem
{
public:
  SpatialAudioSystem() = default;

  // Initialize with the audio engine. _engine may be nullptr (silent mode).
  void Initialize(Neuron::Audio::AudioEngine* _engine);

  // Called each frame to update the listener from the camera.
  void XM_CALLCONV UpdateListener(FXMVECTOR _position, FXMVECTOR _forward, FXMVECTOR _up, float _deltaT);

  // Play a one-shot 3D sound at a world position.
  void XM_CALLCONV PlayOneShot(SoundId _sound, FXMVECTOR _position);

  // Play a one-shot 3D sound with custom volume at a world position.
  void XM_CALLCONV PlayOneShotAtVolume(SoundId _sound, FXMVECTOR _position, float _volume);

  // Play a non-positional (2D) sound (e.g., UI feedback, ambient).
  void PlayUI(SoundId _sound);

  // Load a WAV file into the sound cache.  Call during startup.
  bool LoadSound(SoundId _id, const std::wstring& _filePath);

  // Pre-populate the sound cache from the SoundBank catalog.
  // Silently skips missing files (placeholder sounds not yet created).
  void PreloadFromBank(const SoundBank& _bank);

  // Number of loaded sounds.
  [[nodiscard]] size_t LoadedCount() const noexcept { return m_sounds.size(); }

  // Check if a sound is loaded.
  [[nodiscard]] bool IsLoaded(SoundId _id) const noexcept { return m_sounds.contains(_id); }

private:
  Neuron::Audio::AudioEngine* m_engine = nullptr;
  Neuron::Audio::AudioListener m_listener;

  // Loaded SoundEffect assets keyed by SoundId.
  std::unordered_map<SoundId, std::unique_ptr<Neuron::Audio::SoundEffect>> m_sounds;

  // WAV file data buffers (kept alive while SoundEffect references them).
  std::unordered_map<SoundId, byte_buffer_t> m_wavBuffers;
};
