#pragma once

#include "InputState.h"
#include "GameTypes/EntityHandle.h"
#include "Messages.h"

class ServerConnection;
class ClientWorldState;
class CommandTargeting;
class TargetingUI;
class ChatUI;

namespace Neuron::Graphics { class Camera; }

// InputHandler — consumes InputState to produce game-level commands.
// First-person flight controls (WASD/mouse), fleet commands (right-click),
// and sends them to the server via ServerConnection.

class InputHandler
{
public:
  InputHandler() = default;

  void Initialize(ServerConnection* _connection,
    ClientWorldState* _worldState,
    Neuron::Graphics::Camera* _camera,
    CommandTargeting* _targeting,
    TargetingUI* _targetingUI = nullptr,
    ChatUI* _chatUI = nullptr);

  // Process input for this frame.
  void Update(float _deltaT, const InputState& _input);

  // Currently selected fleet ship handles.
  [[nodiscard]] const std::vector<Neuron::EntityHandle>& GetSelectedShips() const noexcept
  {
    return m_selectedShips;
  }

  // Add/remove ship from selection.
  void SelectShip(Neuron::EntityHandle _handle, bool _addToSelection = false);
  void DeselectAll();

  // Group assignment (Ctrl+1..9) and recall (1..9).
  void AssignGroup(int _groupIndex);
  void RecallGroup(int _groupIndex);

  // Camera sensitivity.
  float MouseSensitivity = 0.003f;
  float MoveSpeed = 50.0f;

private:
  void HandleCameraMovement(float _deltaT, const InputState& _input);
  void HandleFleetCommands(const InputState& _input);
  void SendMoveCommand(const XMFLOAT3& _target);
  void SendAttackCommand(Neuron::EntityHandle _target);

  ServerConnection* m_connection = nullptr;
  ClientWorldState* m_worldState = nullptr;
  Neuron::Graphics::Camera* m_camera = nullptr;
  CommandTargeting* m_targeting = nullptr;
  TargetingUI* m_targetingUI = nullptr;
  ChatUI* m_chatUI = nullptr;

  std::vector<Neuron::EntityHandle> m_selectedShips;

  // Control groups: up to 9 groups.
  static constexpr int MAX_GROUPS = 9;
  std::vector<Neuron::EntityHandle> m_groups[MAX_GROUPS];
};
