#include "pch.h"
#include "CommandTargeting.h"
#include "ClientWorldState.h"
#include "Camera.h"

using namespace Neuron;

void CommandTargeting::Initialize(ClientWorldState* _worldState,
  Neuron::Graphics::Camera* _camera)
{
  m_worldState = _worldState;
  m_camera = _camera;
}

void CommandTargeting::SetScreenSize(float _width, float _height) noexcept
{
  m_screenWidth = _width;
  m_screenHeight = _height;
}

void CommandTargeting::ScreenToRay(float _screenX, float _screenY,
  XMFLOAT3& _outOrigin, XMFLOAT3& _outDirection) const
{
  // Convert screen coords to NDC [-1, 1]
  float ndcX = (2.0f * _screenX / m_screenWidth) - 1.0f;
  float ndcY = 1.0f - (2.0f * _screenY / m_screenHeight);

  // Unproject using inverse view-projection matrix
  // We need to work in world space, not origin-rebased space
  XMMATRIX proj = m_camera->GetProjectionMatrix();
  XMMATRIX view = m_camera->GetViewMatrix();
  XMMATRIX viewProj = XMMatrixMultiply(view, proj);
  XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);

  // Near point in NDC (reverse-Z: near plane is z=1, far plane is z=0)
  XMVECTOR nearPoint = XMVector3TransformCoord(
    XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), invViewProj);
  // Far point in NDC
  XMVECTOR farPoint = XMVector3TransformCoord(
    XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), invViewProj);

  // The ray direction is from near to far, in origin-rebased space.
  // We need to add camera position to get world space.
  XMVECTOR camPos = m_camera->GetPosition();
  XMVECTOR rayOriginWorld = XMVectorAdd(nearPoint, camPos);
  XMVECTOR rayDir = XMVector3Normalize(XMVectorSubtract(farPoint, nearPoint));

  XMStoreFloat3(&_outOrigin, rayOriginWorld);
  XMStoreFloat3(&_outDirection, rayDir);
}

bool CommandTargeting::RaycastFromScreen(float _screenX, float _screenY,
  XMFLOAT3& _outWorldPos,
  EntityHandle& _outHitEntity) const
{
  _outHitEntity = EntityHandle::Invalid();

  XMFLOAT3 origin, direction;
  ScreenToRay(_screenX, _screenY, origin, direction);

  XMVECTOR rayOrigin = XMLoadFloat3(&origin);
  XMVECTOR rayDir = XMLoadFloat3(&direction);

  // First, check against entities (sphere intersection).
  float closestEntityDist = FLT_MAX;
  EntityHandle closestEntity = EntityHandle::Invalid();

  if (m_worldState)
  {
    m_worldState->ForEachActive([&](const ClientEntity& _entity)
    {
      XMVECTOR entityPos = XMLoadFloat3(&_entity.Position);
      XMVECTOR toEntity = XMVectorSubtract(entityPos, rayOrigin);

      // Project entity center onto ray
      float along = XMVectorGetX(XMVector3Dot(toEntity, rayDir));
      if (along < 0.0f)
        return; // Behind ray origin

      // Distance from ray to entity center
      XMVECTOR closestOnRay = XMVectorAdd(rayOrigin, XMVectorScale(rayDir, along));
      float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(entityPos, closestOnRay)));

      // Use a generous pick radius (bounding radius + some screen-space tolerance)
      float pickRadius = 5.0f; // Base pick radius in world units
      if (dist < pickRadius && along < closestEntityDist)
      {
        closestEntityDist = along;
        closestEntity = _entity.Handle;
      }
    });
  }

  if (closestEntity.IsValid())
  {
    _outHitEntity = closestEntity;
    // Return the entity's position as the world target
    if (m_worldState)
    {
      const ClientEntity* entity = m_worldState->GetEntity(closestEntity);
      if (entity)
        _outWorldPos = entity->Position;
    }
    return true;
  }

  // No entity hit — intersect with the tactical grid plane (Y = GridPlaneY).
  float dirY = direction.y;
  if (std::abs(dirY) < 1e-6f)
  {
    // Ray is parallel to the plane — use a far point along the ray
    XMVECTOR farPos = XMVectorAdd(rayOrigin, XMVectorScale(rayDir, 1000.0f));
    XMStoreFloat3(&_outWorldPos, farPos);
    return true;
  }

  float t = (GridPlaneY - origin.y) / dirY;
  if (t < 0.0f)
  {
    // Intersection is behind the camera — use forward projection
    t = 1000.0f;
  }

  XMVECTOR hitPos = XMVectorAdd(rayOrigin, XMVectorScale(rayDir, t));
  XMStoreFloat3(&_outWorldPos, hitPos);
  return true;
}
