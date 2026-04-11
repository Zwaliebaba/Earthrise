#pragma once

#include "GameTypes/EntityHandle.h"
#include "GameTypes/SpaceObjectCategory.h"

struct ClientEntity;
class ClientWorldState;
namespace Neuron::Graphics { class Camera; class ConstantBufferAllocator; class SpriteBatch; }

// TargetingUI — target selection logic and 3D target bracket rendering.
// Supports click-select, Tab-cycle, and nearest-hostile (T key).
// Pure logic methods are unit-testable without GPU resources.
class TargetingUI
{
public:
  TargetingUI() = default;

  void Initialize(ClientWorldState* _worldState,
    Neuron::Graphics::Camera* _camera);

  // Refresh the list of targetable entities from the world state.
  void RefreshTargetList();

  // Cycle to the next target in the list. Returns the new target handle.
  [[nodiscard]] Neuron::EntityHandle CycleNext();

  // Cycle to the previous target in the list.
  [[nodiscard]] Neuron::EntityHandle CyclePrev();

  // Select the nearest entity to the camera. Returns the handle.
  [[nodiscard]] Neuron::EntityHandle SelectNearest();

  // Set target directly (e.g., from click-pick).
  void SetTarget(Neuron::EntityHandle _handle);

  // Clear the current target.
  void ClearTarget() noexcept;

  // Get the current target.
  [[nodiscard]] Neuron::EntityHandle GetTarget() const noexcept { return m_currentTarget; }

  // Render the target bracket around the current target (3D → screen projection).
  void Render(ID3D12GraphicsCommandList* _cmdList,
    Neuron::Graphics::ConstantBufferAllocator& _cbAlloc,
    Neuron::Graphics::SpriteBatch& _spriteBatch,
    UINT _screenWidth, UINT _screenHeight);

  // How many targetable entities in the list.
  [[nodiscard]] size_t TargetCount() const noexcept { return m_targetList.size(); }

  // Current cycle index (-1 if no target).
  [[nodiscard]] int CycleIndex() const noexcept { return m_cycleIndex; }

  // Get the sorted target list (for testing).
  [[nodiscard]] const std::vector<Neuron::EntityHandle>& GetTargetList() const noexcept
  {
    return m_targetList;
  }

  // Filter: which categories are targetable.
  bool TargetShips    = true;
  bool TargetStations = true;
  bool TargetAsteroids = false;

private:
  bool ProjectToScreen(const XMFLOAT3& _worldPos,
    UINT _screenWidth, UINT _screenHeight,
    float& _outX, float& _outY) const;

  bool IsTargetable(Neuron::SpaceObjectCategory _cat) const noexcept;

  ClientWorldState* m_worldState = nullptr;
  Neuron::Graphics::Camera* m_camera = nullptr;

  std::vector<Neuron::EntityHandle> m_targetList;
  Neuron::EntityHandle m_currentTarget;
  int m_cycleIndex = -1;
};
