#pragma once

#include "GameTypes/EntityHandle.h"

struct ClientEntity;
class ClientWorldState;
namespace Neuron::Graphics { class Camera; class ConstantBufferAllocator; class SpriteBatch; }

// JumpgateUI — displays a warp indicator when the player is near a jumpgate.
// Renders a proximity ring and warp-available indicator.
class JumpgateUI
{
public:
  JumpgateUI() = default;

  void Initialize(ClientWorldState* _worldState,
    Neuron::Graphics::Camera* _camera);

  // Update warp availability based on flagship proximity to jumpgates.
  void Update(Neuron::EntityHandle _flagship);

  // Whether a warp is currently available (player near a jumpgate).
  [[nodiscard]] bool IsWarpAvailable() const noexcept { return m_warpAvailable; }

  // The jumpgate entity the player is near (or Invalid).
  [[nodiscard]] Neuron::EntityHandle NearbyJumpgate() const noexcept { return m_nearbyGate; }

  // Safe warp range (same as server DockingRadius default).
  float WarpRange = 100.0f;

  // Check if a specific entity is within warp range of a jumpgate.
  // Returns the jumpgate handle, or Invalid if none in range.
  [[nodiscard]] Neuron::EntityHandle FindNearbyJumpgate(
    Neuron::EntityHandle _shipHandle) const;

  // Render the warp availability indicator.
  void Render(ID3D12GraphicsCommandList* _cmdList,
    Neuron::Graphics::ConstantBufferAllocator& _cbAlloc,
    Neuron::Graphics::SpriteBatch& _spriteBatch,
    UINT _screenWidth, UINT _screenHeight);

private:
  ClientWorldState* m_worldState = nullptr;
  Neuron::Graphics::Camera* m_camera = nullptr;

  bool m_warpAvailable = false;
  Neuron::EntityHandle m_nearbyGate;
};
