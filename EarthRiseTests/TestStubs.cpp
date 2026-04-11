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
