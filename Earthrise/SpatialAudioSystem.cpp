#include "pch.h"
#include "SpatialAudioSystem.h"

using namespace Neuron::Audio;

void SpatialAudioSystem::Initialize(AudioEngine* _engine)
{
  m_engine = _engine;

  // Default listener orientation (forward = -Z, up = +Y)
  m_listener = {};
}

void XM_CALLCONV SpatialAudioSystem::UpdateListener(FXMVECTOR _position, FXMVECTOR _forward, FXMVECTOR _up, float _deltaT)
{
  if (!m_engine) return;

  // Compute velocity from position delta
  XMVECTOR lastPos = XMLoadFloat3(&m_listener.Position);
  if (_deltaT > 0.0f)
  {
    XMVECTOR vel = XMVectorScale(XMVectorSubtract(_position, lastPos), 1.0f / _deltaT);
    m_listener.SetVelocity(vel);
  }

  m_listener.SetPosition(_position);
  m_listener.SetOrientation(_forward, _up);
}

void XM_CALLCONV SpatialAudioSystem::PlayOneShot(SoundId _sound, FXMVECTOR _position)
{
  PlayOneShotAtVolume(_sound, _position, 1.0f);
}

void XM_CALLCONV SpatialAudioSystem::PlayOneShotAtVolume(SoundId _sound, FXMVECTOR _position, float _volume)
{
  if (!m_engine) return;

  auto it = m_sounds.find(_sound);
  if (it == m_sounds.end()) return;

  auto instance = it->second->CreateInstance(SoundEffectInstance_Use3D);
  if (!instance) return;

  instance->SetVolume(_volume);

  AudioEmitter emitter;
  emitter.SetPosition(_position);

  instance->Apply3D(m_listener, emitter);
  instance->Play();
  // Instance is fire-and-forget; the engine tracks it via one-shot pool.
}

void SpatialAudioSystem::PlayUI(SoundId _sound)
{
  if (!m_engine) return;

  auto it = m_sounds.find(_sound);
  if (it == m_sounds.end()) return;

  // Non-positional one-shot
  it->second->Play();
}

bool SpatialAudioSystem::LoadSound(SoundId _id, const std::wstring& _filePath)
{
  if (!m_engine) return false;

  try
  {
    auto data = BinaryFile::ReadFile(_filePath);
    if (data.empty()) return false;

    auto effect = std::make_unique<SoundEffect>(m_engine, data);
    m_wavBuffers[_id] = std::move(data);
    m_sounds[_id] = std::move(effect);
    return true;
  }
  catch (...)
  {
    return false;
  }
}

void SpatialAudioSystem::PreloadFromBank(const SoundBank& _bank)
{
  for (const auto& [id, entry] : _bank.GetAll())
  {
    // Build full path: GameData/Audio/<filename>
    std::wstring path = FileSys::GetHomeDirectory() + L"Audio\\" +
      std::wstring(entry.FilePath.begin(), entry.FilePath.end());

    LoadSound(id, path);
  }
}
