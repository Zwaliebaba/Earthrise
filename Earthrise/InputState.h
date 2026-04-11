#pragma once

// InputState.h — Per-frame input state abstraction layer.
// Captures keyboard, mouse, and touch input from Windows messages.
// Provides polling API for game systems.

enum class InputAction : uint8_t
{
  MoveForward,
  MoveBackward,
  MoveLeft,
  MoveRight,
  MoveUp,
  MoveDown,
  SelectUnit,
  DeselectAll,
  GroupAssign,     // Ctrl+1..9
  GroupRecall,     // 1..9
  ZoomTactical,
  ToggleTacticalGrid,
  IssueCommand,    // Right-click — move-to or attack
  CameraAttach,    // Attach camera to selected unit
  COUNT
};

struct TouchPoint
{
  uint32_t PointerId = 0;
  float X = 0.0f;
  float Y = 0.0f;
  bool IsDown = false;
};

class InputState
{
public:
  InputState() = default;

  // Call at the start of each frame to prepare for new input.
  void BeginFrame();

  // Process a Windows message. Returns true if the message was consumed.
  bool ProcessMessage(UINT _message, WPARAM _wParam, LPARAM _lParam);

  // Key state polling.
  [[nodiscard]] bool IsKeyDown(int _vk) const noexcept;
  [[nodiscard]] bool IsKeyPressed(int _vk) const noexcept;  // Down this frame, up last frame
  [[nodiscard]] bool IsKeyReleased(int _vk) const noexcept; // Up this frame, down last frame

  // Mouse state.
  [[nodiscard]] float GetMouseX() const noexcept { return m_mouseX; }
  [[nodiscard]] float GetMouseY() const noexcept { return m_mouseY; }
  [[nodiscard]] float GetMouseDeltaX() const noexcept { return m_mouseDeltaX; }
  [[nodiscard]] float GetMouseDeltaY() const noexcept { return m_mouseDeltaY; }
  [[nodiscard]] float GetScrollDelta() const noexcept { return m_scrollDelta; }
  [[nodiscard]] bool IsLeftMouseDown() const noexcept { return m_leftMouseDown; }
  [[nodiscard]] bool IsRightMouseDown() const noexcept { return m_rightMouseDown; }
  [[nodiscard]] bool IsLeftMousePressed() const noexcept { return m_leftMouseDown && !m_prevLeftMouseDown; }
  [[nodiscard]] bool IsRightMousePressed() const noexcept { return m_rightMouseDown && !m_prevRightMouseDown; }

  // Touch state (WM_POINTER).
  [[nodiscard]] const std::vector<TouchPoint>& GetTouchPoints() const noexcept { return m_touchPoints; }
  [[nodiscard]] size_t GetTouchCount() const noexcept { return m_touchPoints.size(); }

  // Modifier keys.
  [[nodiscard]] bool IsCtrlDown() const noexcept { return IsKeyDown(VK_CONTROL); }
  [[nodiscard]] bool IsShiftDown() const noexcept { return IsKeyDown(VK_SHIFT); }
  [[nodiscard]] bool IsAltDown() const noexcept { return IsKeyDown(VK_MENU); }

  // Map raw input to abstract action (convenience).
  [[nodiscard]] bool IsActionActive(InputAction _action) const noexcept;

  // Screen dimensions (set by game app on resize).
  void SetScreenSize(float _width, float _height) noexcept;
  [[nodiscard]] float GetScreenWidth() const noexcept { return m_screenWidth; }
  [[nodiscard]] float GetScreenHeight() const noexcept { return m_screenHeight; }

private:
  // Keyboard state — 256 virtual key codes
  bool m_keyDown[256] = {};
  bool m_prevKeyDown[256] = {};

  // Mouse state
  float m_mouseX = 0.0f;
  float m_mouseY = 0.0f;
  float m_prevMouseX = 0.0f;
  float m_prevMouseY = 0.0f;
  float m_mouseDeltaX = 0.0f;
  float m_mouseDeltaY = 0.0f;
  float m_scrollDelta = 0.0f;
  bool m_leftMouseDown = false;
  bool m_rightMouseDown = false;
  bool m_prevLeftMouseDown = false;
  bool m_prevRightMouseDown = false;

  // Touch state (via WM_POINTER)
  std::vector<TouchPoint> m_touchPoints;

  // Screen dimensions
  float m_screenWidth = 1920.0f;
  float m_screenHeight = 1080.0f;
};
