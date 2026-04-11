#pragma once

#include "GameTypes/EntityHandle.h"

class ClientWorldState;
class InputState;
namespace Neuron::Graphics { class Camera; class ConstantBufferAllocator; class SpriteBatch; }

// FleetSelectionUI — renders selection brackets around owned ships,
// handles multi-select (click, Ctrl+click, box-drag in tactical view),
// and displays selection state.

class FleetSelectionUI
{
public:
  FleetSelectionUI() = default;

  void Initialize(ClientWorldState* _worldState,
    Neuron::Graphics::Camera* _camera);

  // Update box-drag state from input.
  void Update(float _deltaT, const InputState& _input);

  // Render selection brackets and drag box.
  void Render(ID3D12GraphicsCommandList* _cmdList,
    Neuron::Graphics::ConstantBufferAllocator& _cbAlloc,
    Neuron::Graphics::SpriteBatch& _spriteBatch,
    UINT _screenWidth, UINT _screenHeight);

  // Check if a screen-space click hits a world-space entity (sphere test).
  // Returns the handle of the closest hit entity, or Invalid.
  [[nodiscard]] Neuron::EntityHandle PickEntity(
    float _screenX, float _screenY) const;

  // Get entities within a screen-space box.
  void BoxSelect(float _x0, float _y0, float _x1, float _y1,
    std::vector<Neuron::EntityHandle>& _outHandles) const;

  // Set the selected handles (rendered with brackets).
  void SetSelection(const std::vector<Neuron::EntityHandle>& _selection);

  // Is the user currently drag-selecting?
  [[nodiscard]] bool IsDragging() const noexcept { return m_isDragging; }

private:
  // Project a world position to screen space. Returns false if behind camera.
  [[nodiscard]] bool ProjectToScreen(const XMFLOAT3& _worldPos,
    UINT _screenWidth, UINT _screenHeight,
    float& _outX, float& _outY) const;

  ClientWorldState* m_worldState = nullptr;
  Neuron::Graphics::Camera* m_camera = nullptr;

  // Selection state
  std::vector<Neuron::EntityHandle> m_selectedHandles;

  // Box-drag state
  bool m_isDragging = false;
  float m_dragStartX = 0.0f;
  float m_dragStartY = 0.0f;
  float m_dragEndX = 0.0f;
  float m_dragEndY = 0.0f;

  static constexpr float SELECT_RADIUS_SCREEN = 20.0f; // Pixel radius for click-select
  static constexpr float DRAG_THRESHOLD = 5.0f;       // Min pixels before box-drag
};
