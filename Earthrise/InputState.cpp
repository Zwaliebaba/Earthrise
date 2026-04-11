#include "pch.h"
#include <windowsx.h>
#include "InputState.h"

void InputState::BeginFrame()
{
  // Compute mouse delta from last frame
  m_mouseDeltaX = m_mouseX - m_prevMouseX;
  m_mouseDeltaY = m_mouseY - m_prevMouseY;
  m_prevMouseX = m_mouseX;
  m_prevMouseY = m_mouseY;

  // Reset per-frame accumulators
  m_scrollDelta = 0.0f;
  m_touchPoints.clear();
}

void InputState::EndFrame()
{
  // Snapshot current state as previous — must be called after Update/Render
  // and before the next frame's messages arrive.
  std::memcpy(m_prevKeyDown, m_keyDown, sizeof(m_keyDown));
  m_prevLeftMouseDown = m_leftMouseDown;
  m_prevRightMouseDown = m_rightMouseDown;
}

bool InputState::ProcessMessage(UINT _message, WPARAM _wParam, LPARAM _lParam)
{
  switch (_message)
  {
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    if (_wParam < 256)
      m_keyDown[_wParam] = true;
    return true;

  case WM_KEYUP:
  case WM_SYSKEYUP:
    if (_wParam < 256)
      m_keyDown[_wParam] = false;
    return true;

  case WM_MOUSEMOVE:
    m_mouseX = static_cast<float>(GET_X_LPARAM(_lParam));
    m_mouseY = static_cast<float>(GET_Y_LPARAM(_lParam));
    return true;

  case WM_LBUTTONDOWN:
    m_leftMouseDown = true;
    return true;

  case WM_LBUTTONUP:
    m_leftMouseDown = false;
    return true;

  case WM_RBUTTONDOWN:
    m_rightMouseDown = true;
    // Capture the mouse so we keep receiving WM_MOUSEMOVE while looking around,
    // even when the cursor moves outside the client area.
    SetCapture(ClientEngine::Window());
    ShowCursor(FALSE);
    return true;

  case WM_RBUTTONUP:
    m_rightMouseDown = false;
    ReleaseCapture();
    ShowCursor(TRUE);
    return true;

  case WM_MOUSEWHEEL:
    m_scrollDelta += static_cast<float>(GET_WHEEL_DELTA_WPARAM(_wParam)) / WHEEL_DELTA;
    return true;

  case WM_POINTERDOWN:
  case WM_POINTERUPDATE:
  case WM_POINTERUP:
  {
    uint32_t pointerId = GET_POINTERID_WPARAM(_wParam);
    POINT pt;
    pt.x = GET_X_LPARAM(_lParam);
    pt.y = GET_Y_LPARAM(_lParam);

    if (_message == WM_POINTERUP)
    {
      // Remove touch point
      std::erase_if(m_touchPoints,
        [pointerId](const TouchPoint& tp) { return tp.PointerId == pointerId; });
    }
    else
    {
      // Add or update touch point
      bool found = false;
      for (auto& tp : m_touchPoints)
      {
        if (tp.PointerId == pointerId)
        {
          tp.X = static_cast<float>(pt.x);
          tp.Y = static_cast<float>(pt.y);
          tp.IsDown = (_message == WM_POINTERDOWN || _message == WM_POINTERUPDATE);
          found = true;
          break;
        }
      }
      if (!found)
      {
        TouchPoint tp;
        tp.PointerId = pointerId;
        tp.X = static_cast<float>(pt.x);
        tp.Y = static_cast<float>(pt.y);
        tp.IsDown = true;
        m_touchPoints.push_back(tp);
      }
    }
    return true;
  }

  default:
    return false;
  }
}

bool InputState::IsKeyDown(int _vk) const noexcept
{
  if (_vk < 0 || _vk >= 256) return false;
  return m_keyDown[_vk];
}

bool InputState::IsKeyPressed(int _vk) const noexcept
{
  if (_vk < 0 || _vk >= 256) return false;
  return m_keyDown[_vk] && !m_prevKeyDown[_vk];
}

bool InputState::IsKeyReleased(int _vk) const noexcept
{
  if (_vk < 0 || _vk >= 256) return false;
  return !m_keyDown[_vk] && m_prevKeyDown[_vk];
}

bool InputState::IsActionActive(InputAction _action) const noexcept
{
  switch (_action)
  {
  case InputAction::MoveForward:        return IsKeyDown('W');
  case InputAction::MoveBackward:       return IsKeyDown('S');
  case InputAction::MoveLeft:           return IsKeyDown('A');
  case InputAction::MoveRight:          return IsKeyDown('D');
  case InputAction::MoveUp:             return IsKeyDown(VK_SPACE);
  case InputAction::MoveDown:           return IsKeyDown('X');
  case InputAction::SelectUnit:         return IsLeftMousePressed();
  case InputAction::DeselectAll:        return IsKeyPressed(VK_ESCAPE);
  case InputAction::ZoomTactical:       return GetScrollDelta() != 0.0f;
  case InputAction::ToggleTacticalGrid: return IsKeyPressed('G');
  case InputAction::IssueCommand:       return IsRightMousePressed();
  case InputAction::CameraAttach:       return IsKeyPressed('F');
  case InputAction::TargetCycleNext:    return IsKeyPressed(VK_TAB) && !IsShiftDown();
  case InputAction::TargetCyclePrev:    return IsKeyPressed(VK_TAB) && IsShiftDown();
  case InputAction::TargetNearest:      return IsKeyPressed('T');
  case InputAction::ChatToggle:         return IsKeyPressed(VK_RETURN);
  default: return false;
  }
}

void InputState::SetScreenSize(float _width, float _height) noexcept
{
  m_screenWidth = _width;
  m_screenHeight = _height;
}
