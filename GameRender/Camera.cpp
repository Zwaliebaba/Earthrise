#include "pch.h"
#include "Camera.h"

using namespace Neuron::Graphics;

Camera::Camera() noexcept
{
  XMStoreFloat4x4(&m_viewMatrix, XMMatrixIdentity());
  XMStoreFloat4x4(&m_projMatrix, XMMatrixIdentity());
  RebuildProjectionMatrix();
  RebuildViewMatrix();
}

void Camera::SetPerspective(float fovY, float aspectRatio, float nearZ, float farZ) noexcept
{
  m_fovY = fovY;
  m_aspect = aspectRatio;
  m_nearZ = nearZ;
  m_farZ = farZ;
  m_projDirty = true;
}

void Camera::SetPosition(FXMVECTOR position) noexcept
{
  XMStoreFloat3(&m_worldPosition, position);
  m_viewDirty = true;
}

void Camera::SetLookDirection(FXMVECTOR forward, FXMVECTOR up) noexcept
{
  XMStoreFloat3(&m_forward, XMVector3Normalize(forward));
  XMStoreFloat3(&m_up, XMVector3Normalize(up));
  m_viewDirty = true;
}

void Camera::SetChaseTarget(FXMVECTOR targetPos, FXMVECTOR targetForward) noexcept
{
  XMStoreFloat3(&m_chaseTargetPos, targetPos);
  XMStoreFloat3(&m_chaseTargetForward, XMVector3Normalize(targetForward));
}

XMVECTOR Camera::GetRight() const noexcept
{
  return XMVector3Normalize(XMVector3Cross(GetUp(), GetForward()));
}

void Camera::Update(float deltaT) noexcept
{
  switch (m_mode)
  {
  case CameraMode::ChaseCamera:
  {
    // Position behind and above the target
    XMVECTOR targetPos = XMLoadFloat3(&m_chaseTargetPos);
    XMVECTOR targetFwd = XMLoadFloat3(&m_chaseTargetForward);

    XMVECTOR offset = XMVectorScale(targetFwd, -m_chaseDistance * m_zoomLevel);
    offset = XMVectorAdd(offset, XMVectorSet(0, m_chaseHeight * m_zoomLevel, 0, 0));

    XMVECTOR desiredPos = XMVectorAdd(targetPos, offset);

    // Smooth interpolation
    XMVECTOR currentPos = GetPosition();
    XMVECTOR smoothed = XMVectorLerp(currentPos, desiredPos, std::min(deltaT * 5.0f, 1.0f));
    SetPosition(smoothed);

    XMVECTOR lookDir = XMVector3Normalize(XMVectorSubtract(targetPos, smoothed));
    SetLookDirection(lookDir);
    break;
  }

  case CameraMode::FreeFly:
    // Direction handled by Rotate/Translate calls from input system
    break;

  case CameraMode::TacticalZoom:
    // High-angle view, controlled by zoom level
    break;
  }

  if (m_projDirty)
    RebuildProjectionMatrix();
  if (m_viewDirty)
    RebuildViewMatrix();
}

void Camera::Rotate(float yaw, float pitch) noexcept
{
  m_yaw += yaw;
  m_pitch += pitch;

  // Clamp pitch to avoid flipping
  m_pitch = std::clamp(m_pitch, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f);

  // Reconstruct forward from yaw/pitch
  XMFLOAT3 forward;
  forward.x = cosf(m_pitch) * sinf(m_yaw);
  forward.y = sinf(m_pitch);
  forward.z = cosf(m_pitch) * cosf(m_yaw);

  XMStoreFloat3(&m_forward, XMVector3Normalize(XMLoadFloat3(&forward)));
  m_viewDirty = true;
}

void Camera::Translate(FXMVECTOR localDirection, float distance) noexcept
{
  XMVECTOR fwd = GetForward();
  XMVECTOR right = GetRight();
  XMVECTOR up = Math::Vector3::UP;

  // localDirection.x = right, .y = up, .z = forward
  XMVECTOR movement = XMVectorScale(right, XMVectorGetX(localDirection));
  movement = XMVectorAdd(movement, XMVectorScale(up, XMVectorGetY(localDirection)));
  movement = XMVectorAdd(movement, XMVectorScale(fwd, XMVectorGetZ(localDirection)));
  movement = XMVectorScale(movement, distance);

  SetPosition(XMVectorAdd(GetPosition(), movement));
}

void Camera::Zoom(float delta) noexcept
{
  m_zoomLevel = std::clamp(m_zoomLevel + delta, MIN_ZOOM, MAX_ZOOM);
}

void Camera::RebuildViewMatrix() noexcept
{
  // Origin-rebased: camera is at origin, looking along forward
  XMVECTOR eye = XMVectorZero();
  XMVECTOR at = XMLoadFloat3(&m_forward);
  XMVECTOR up = XMLoadFloat3(&m_up);

  XMStoreFloat4x4(&m_viewMatrix, XMMatrixLookToLH(eye, at, up));

  // Update frustum in origin-rebased space
  // Use standard (non-reversed) projection for frustum — BoundingFrustum
  // cannot handle the swapped near/far of a reverse-Z matrix.
  XMMATRIX standardProj = XMMatrixPerspectiveFovLH(m_fovY, m_aspect, m_nearZ, m_farZ);
  BoundingFrustum::CreateFromMatrix(m_frustum, standardProj);
  XMMATRIX invView = XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_viewMatrix));
  m_frustum.Transform(m_frustum, invView);

  m_viewDirty = false;
}

void Camera::RebuildProjectionMatrix() noexcept
{
  // Reverse-Z: swap near and far in the projection matrix.
  // Near plane maps to Z=1, far plane maps to Z=0.
  // This gives near-logarithmic depth precision distribution.
  XMStoreFloat4x4(&m_projMatrix,
    XMMatrixPerspectiveFovLH(m_fovY, m_aspect, m_farZ, m_nearZ));

  m_projDirty = false;
  m_viewDirty = true; // Frustum rebuild happens in RebuildViewMatrix
}

XMMATRIX Camera::GetViewMatrix() const noexcept
{
  return XMLoadFloat4x4(&m_viewMatrix);
}

XMMATRIX Camera::GetProjectionMatrix() const noexcept
{
  return XMLoadFloat4x4(&m_projMatrix);
}

XMMATRIX Camera::GetViewProjectionMatrix() const noexcept
{
  return XMMatrixMultiply(GetViewMatrix(), GetProjectionMatrix());
}

bool Camera::IsVisible(FXMVECTOR rebasedCenter, float radius) const noexcept
{
  BoundingSphere sphere;
  XMStoreFloat3(&sphere.Center, rebasedCenter);
  sphere.Radius = radius;
  return m_frustum.Contains(sphere) != DISJOINT;
}
