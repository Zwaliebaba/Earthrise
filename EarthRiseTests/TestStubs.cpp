// TestStubs.cpp — Stub implementations of GPU-dependent classes so that
// Earthrise UI .cpp files can be compiled into the test DLL without pulling
// in the full rendering pipeline.  Only the linker-required symbols are
// provided — the stubs are never called at runtime from tests.

#include "pch.h"
#include "SpriteBatch.h"
#include "Camera.h"

using namespace Neuron::Graphics;

// ── SpriteBatch stubs ─────────────────────────────────────────────────────
void SpriteBatch::Initialize() {}
void SpriteBatch::Begin(ID3D12GraphicsCommandList*, ConstantBufferAllocator&, UINT, UINT) {}
void SpriteBatch::DrawRect(const RECT&, FXMVECTOR) {}
void SpriteBatch::End() {}
void SpriteBatch::FlushBatch() {}

// ── Camera stubs ──────────────────────────────────────────────────────────
Camera::Camera() noexcept {}
void Camera::SetPerspective(float, float, float, float) noexcept {}
void Camera::SetPosition(FXMVECTOR) noexcept {}
void Camera::SetLookDirection(FXMVECTOR, FXMVECTOR) noexcept {}
void Camera::Update(float) noexcept {}
XMMATRIX Camera::GetViewMatrix() const noexcept { return XMMatrixIdentity(); }
XMMATRIX Camera::GetProjectionMatrix() const noexcept { return XMMatrixIdentity(); }
XMMATRIX Camera::GetViewProjectionMatrix() const noexcept { return XMMatrixIdentity(); }
XMVECTOR Camera::GetRight() const noexcept { return XMVectorSet(1, 0, 0, 0); }
bool Camera::IsVisible(FXMVECTOR, float) const noexcept { return true; }
void Camera::SetChaseTarget(FXMVECTOR, FXMVECTOR) noexcept {}
void Camera::Rotate(float, float) noexcept {}
void Camera::Translate(FXMVECTOR, float) noexcept {}
void Camera::Zoom(float) noexcept {}
void Camera::RebuildViewMatrix() noexcept {}
void Camera::RebuildProjectionMatrix() noexcept {}

// ── Audio stubs (Phase 9) ─────────────────────────────────────────────────
#include "Audio.h"

using namespace Neuron::Audio;

SoundEffect::SoundEffect(AudioEngine*, const byte_buffer_t&) : mWaveFormat(nullptr), mStartAudio(nullptr), mAudioBytes(0), mLoopStart(0), mLoopLength(0), mEngine(nullptr), mOneShots(0) {}
SoundEffect::SoundEffect(const AudioEngine*, const byte_buffer_t&, const WAVEFORMATEX*, const uint8_t*, size_t) : mWaveFormat(nullptr), mStartAudio(nullptr), mAudioBytes(0), mLoopStart(0), mLoopLength(0), mEngine(nullptr), mOneShots(0) {}
SoundEffect::SoundEffect(const AudioEngine*, const byte_buffer_t&, const WAVEFORMATEX*, const uint8_t*, size_t, uint32_t, uint32_t) : mWaveFormat(nullptr), mStartAudio(nullptr), mAudioBytes(0), mLoopStart(0), mLoopLength(0), mEngine(nullptr), mOneShots(0) {}
SoundEffect::~SoundEffect() {}
void SoundEffect::Play() {}
bool SoundEffect::IsInUse() const { return false; }
size_t SoundEffect::GetSampleSizeInBytes() const { return 0; }
size_t SoundEffect::GetSampleDuration() const { return 0; }
size_t SoundEffect::GetSampleDurationMS() const { return 0; }
const WAVEFORMATEX* SoundEffect::GetFormat() const { return nullptr; }
void SoundEffect::FillSubmitBuffer(XAUDIO2_BUFFER&) const {}
std::unique_ptr<SoundEffectInstance> SoundEffect::CreateInstance(SOUND_EFFECT_INSTANCE_FLAGS) { return nullptr; }

SoundEffectInstance::~SoundEffectInstance() {}
void SoundEffectInstance::Play(bool) {}
void SoundEffectInstance::Stop(bool) {}
void SoundEffectInstance::Pause() {}
void SoundEffectInstance::Resume() {}
void SoundEffectInstance::SetVolume(float) {}
void SoundEffectInstance::SetPitch(float) {}
void SoundEffectInstance::SetPan(float) {}
void SoundEffectInstance::Apply3D(const AudioListener&, const AudioEmitter&) {}
bool SoundEffectInstance::IsLooped() const { return false; }
SoundState SoundEffectInstance::GetState() { return STOPPED; }
void SoundEffectInstance::OnDestroyParent() {}

// ── SoundCommon stubs ─────────────────────────────────────────────────────
void SoundEffectInstanceBase::SetPan(float) {}
void SoundEffectInstanceBase::Apply3D(const AudioListener&, const AudioEmitter&) {}

// ── AudioEngine stubs (only methods referenced by SoundEffectInstanceBase inlines) ──
// Cannot define ctor/dtor due to PIMPL with incomplete Impl type.
uint32_t AudioEngine::GetChannelMask() const { return 0; }
int AudioEngine::GetOutputChannels() const { return 2; }
void AudioEngine::DestroyVoice(IXAudio2SourceVoice*) {}
IXAudio2MasteringVoice* AudioEngine::GetMasterVoice() const { return nullptr; }
IXAudio2SubmixVoice* AudioEngine::GetReverbVoice() const { return nullptr; }
void AudioEngine::AllocateVoice(const WAVEFORMATEX*, SOUND_EFFECT_INSTANCE_FLAGS, bool, IXAudio2SourceVoice**) {}
void AudioEngine::RegisterNotify(IVoiceNotify*, bool) {}
void AudioEngine::UnregisterNotify(IVoiceNotify*, bool, bool) {}

// ── FileSys stubs (BinaryFile::ReadFile needed by SpatialAudioSystem) ─────
namespace Neuron
{
  byte_buffer_t BinaryFile::ReadFile(const std::wstring&) { return {}; }
}
