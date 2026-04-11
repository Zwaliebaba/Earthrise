#include "pch.h"
#include "InputHandler.h"
#include "ServerConnection.h"
#include "ClientWorldState.h"
#include "CommandTargeting.h"
#include "TargetingUI.h"
#include "ChatUI.h"
#include "Camera.h"

using namespace Neuron;

void InputHandler::Initialize(ServerConnection* _connection,
  ClientWorldState* _worldState,
  Neuron::Graphics::Camera* _camera,
  CommandTargeting* _targeting,
  TargetingUI* _targetingUI,
  ChatUI* _chatUI)
{
  m_connection  = _connection;
  m_worldState  = _worldState;
  m_camera      = _camera;
  m_targeting   = _targeting;
  m_targetingUI = _targetingUI;
  m_chatUI      = _chatUI;
}

void InputHandler::Update(float _deltaT, const InputState& _input)
{
  // Chat input mode consumes keyboard input
  if (m_chatUI && m_chatUI->IsTyping())
  {
    // Enter commits the message
    if (_input.IsActionActive(InputAction::ChatToggle))
    {
      std::string text = m_chatUI->CommitInput();
      if (!text.empty() && m_connection)
      {
        ChatMsg msg;
        msg.Channel = m_chatUI->ActiveChannel;
        msg.SenderName = "Player1";
        msg.Text = std::move(text);
        DataWriter writer;
        msg.Write(writer);
        m_connection->SendReliable(MessageId::ChatMessage, writer.Data(), writer.Size());
      }
    }
    // Backspace
    else if (_input.IsKeyPressed(VK_BACK))
    {
      m_chatUI->Backspace();
    }
    // Escape cancels typing
    else if (_input.IsKeyPressed(VK_ESCAPE))
    {
      m_chatUI->ToggleTyping();
    }
    // Type printable characters
    else
    {
      for (int vk = 0x20; vk <= 0x7E; ++vk)
      {
        if (_input.IsKeyPressed(vk))
        {
          char c = static_cast<char>(vk);
          if (!_input.IsShiftDown() && c >= 'A' && c <= 'Z')
            c = c - 'A' + 'a'; // Lowercase
          m_chatUI->TypeChar(c);
        }
      }
    }
    return; // Don't process game input while typing
  }

  // Toggle chat mode
  if (m_chatUI && _input.IsActionActive(InputAction::ChatToggle))
    m_chatUI->ToggleTyping();

  // Target cycling
  if (m_targetingUI)
  {
    if (_input.IsActionActive(InputAction::TargetCycleNext))
      m_targetingUI->CycleNext();
    else if (_input.IsActionActive(InputAction::TargetCyclePrev))
      m_targetingUI->CyclePrev();
    else if (_input.IsActionActive(InputAction::TargetNearest))
      m_targetingUI->SelectNearest();
  }

  HandleCameraMovement(_deltaT, _input);
  HandleFleetCommands(_input);

  // Group assignment (Ctrl+1..9)
  if (_input.IsCtrlDown())
  {
    for (int i = 0; i < MAX_GROUPS; ++i)
    {
      if (_input.IsKeyPressed('1' + i))
        AssignGroup(i);
    }
  }
  else
  {
    // Group recall (1..9)
    for (int i = 0; i < MAX_GROUPS; ++i)
    {
      if (_input.IsKeyPressed('1' + i))
        RecallGroup(i);
    }
  }

  // Deselect all
  if (_input.IsActionActive(InputAction::DeselectAll))
    DeselectAll();

  // Tactical zoom
  float scroll = _input.GetScrollDelta();
  if (scroll != 0.0f && m_camera)
    m_camera->Zoom(scroll);
}

void InputHandler::HandleCameraMovement(float _deltaT, const InputState& _input)
{
  if (!m_camera) return;

  // Mouse look (when right mouse button held or in free-fly mode)
  // Mouse look only while right button is held.
  if (_input.IsRightMouseDown())
  {
    float dx = _input.GetMouseDeltaX() * MouseSensitivity;
    float dy = _input.GetMouseDeltaY() * MouseSensitivity;
    if (dx != 0.0f || dy != 0.0f)
      m_camera->Rotate(dx, dy);
  }

  // WASD movement
  XMVECTOR moveDir = XMVectorZero();

  if (_input.IsActionActive(InputAction::MoveForward))
    moveDir = XMVectorAdd(moveDir, Math::Vector3::FORWARD);
  if (_input.IsActionActive(InputAction::MoveBackward))
    moveDir = XMVectorAdd(moveDir, Math::Vector3::BACKWARD);
  if (_input.IsActionActive(InputAction::MoveLeft))
    moveDir = XMVectorAdd(moveDir, Math::Vector3::LEFT);
  if (_input.IsActionActive(InputAction::MoveRight))
    moveDir = XMVectorAdd(moveDir, Math::Vector3::RIGHT);
  if (_input.IsActionActive(InputAction::MoveUp))
    moveDir = XMVectorAdd(moveDir, Math::Vector3::UP);
  if (_input.IsActionActive(InputAction::MoveDown))
    moveDir = XMVectorAdd(moveDir, Math::Vector3::DOWN);

  if (!XMVector3Equal(moveDir, XMVectorZero()))
  {
    moveDir = XMVector3Normalize(moveDir);
    m_camera->Translate(moveDir, MoveSpeed * _deltaT);
  }

  // Camera attach to selected ship (F key)
  if (_input.IsActionActive(InputAction::CameraAttach) && !m_selectedShips.empty() && m_worldState)
  {
    const ClientEntity* entity = m_worldState->GetEntity(m_selectedShips[0]);
    if (entity)
    {
      m_camera->SetChaseTarget(XMLoadFloat3(&entity->Position),
        XMVectorSet(0, 0, 1, 0));
      m_camera->SetMode(Graphics::CameraMode::ChaseCamera);
    }
  }
}

void InputHandler::HandleFleetCommands(const InputState& _input)
{
  if (!m_connection || !m_targeting) return;

  // Right-click: issue move or attack command
  if (_input.IsRightMousePressed())
  {
    XMFLOAT3 worldTarget;
    EntityHandle hitEntity;

    if (m_targeting->RaycastFromScreen(_input.GetMouseX(), _input.GetMouseY(),
      worldTarget, hitEntity))
    {
      if (hitEntity.IsValid())
        SendAttackCommand(hitEntity);
      else
        SendMoveCommand(worldTarget);
    }
  }
}

void InputHandler::SendMoveCommand(const XMFLOAT3& _target)
{
  if (!m_connection || m_selectedShips.empty()) return;

  DataWriter writer;
  PlayerCommandMsg cmd;
  cmd.Type = CommandType::MoveTo;
  cmd.TargetPosition = _target;
  cmd.Write(writer);
  m_connection->SendReliable(MessageId::PlayerCommand, writer.Data(), writer.Size());
}

void InputHandler::SendAttackCommand(EntityHandle _target)
{
  if (!m_connection || m_selectedShips.empty()) return;

  DataWriter writer;
  PlayerCommandMsg cmd;
  cmd.Type = CommandType::AttackTarget;
  cmd.TargetHandle = _target;
  cmd.Write(writer);
  m_connection->SendReliable(MessageId::PlayerCommand, writer.Data(), writer.Size());
}

void InputHandler::SelectShip(EntityHandle _handle, bool _addToSelection)
{
  if (!_addToSelection)
    m_selectedShips.clear();

  // Don't add duplicates
  for (const auto& h : m_selectedShips)
    if (h == _handle) return;

  m_selectedShips.push_back(_handle);
}

void InputHandler::DeselectAll()
{
  m_selectedShips.clear();
}

void InputHandler::AssignGroup(int _groupIndex)
{
  if (_groupIndex >= 0 && _groupIndex < MAX_GROUPS)
    m_groups[_groupIndex] = m_selectedShips;
}

void InputHandler::RecallGroup(int _groupIndex)
{
  if (_groupIndex >= 0 && _groupIndex < MAX_GROUPS)
    m_selectedShips = m_groups[_groupIndex];
}
