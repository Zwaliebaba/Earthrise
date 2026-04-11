#pragma once

#include "GameTypes/EntityHandle.h"

class ClientWorldState;
namespace Neuron::Graphics { class Camera; }

// CommandTargeting — ray-cast from camera through mouse position into 3D space.
// Used to determine move destinations and attack targets for fleet commands.

class CommandTargeting
{
public:
  CommandTargeting() = default;

  void Initialize(ClientWorldState* _worldState,
    Neuron::Graphics::Camera* _camera);

  // Set screen dimensions (call on resize).
  void SetScreenSize(float _width, float _height) noexcept;

  // Cast a ray from screen coordinates into 3D space.
  // Returns true if a valid target was found.
  // _outWorldPos: the world-space intersection point (on grid plane or entity).
  // _outHitEntity: entity handle if an entity was hit, Invalid otherwise.
  bool RaycastFromScreen(float _screenX, float _screenY,
    XMFLOAT3& _outWorldPos,
    Neuron::EntityHandle& _outHitEntity) const;

  // Compute a world-space ray from screen coordinates.
  // _outOrigin: ray origin (camera world position).
  // _outDirection: normalized ray direction.
  void ScreenToRay(float _screenX, float _screenY,
    XMFLOAT3& _outOrigin, XMFLOAT3& _outDirection) const;

  // Height of the tactical command plane (Y-level for move-to commands).
  float GridPlaneY = 0.0f;

private:
  ClientWorldState* m_worldState = nullptr;
  Neuron::Graphics::Camera* m_camera = nullptr;
  float m_screenWidth = 1920.0f;
  float m_screenHeight = 1080.0f;
};
