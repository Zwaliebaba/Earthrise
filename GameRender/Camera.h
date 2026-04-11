#pragma once

namespace Neuron::Graphics
{
  enum class CameraMode : uint8_t
  {
    FreeFly,        // Free-fly camera (WASD + mouse)
    ChaseCamera,    // Chase cam attached to flagship
    TacticalZoom,   // Scroll-wheel tactical view
  };

  // Camera system with reverse-Z projection and origin-rebasing.
  // Origin-rebasing: subtracts camera world position from all entity positions
  // before building the view matrix, keeping GPU-side coordinates near the origin
  // to prevent float precision loss at the 100 km zone edges.
  class Camera
  {
  public:
    Camera() noexcept;

    void SetPerspective(float fovY, float aspectRatio, float nearZ, float farZ) noexcept;
    void SetPosition(FXMVECTOR position) noexcept;
    void SetLookDirection(FXMVECTOR forward, FXMVECTOR up = Math::Vector3::UP) noexcept;
    void SetMode(CameraMode mode) noexcept { m_mode = mode; }

    void Update(float deltaT) noexcept;

    // Get the view matrix (origin-rebased — camera at origin)
    [[nodiscard]] XMMATRIX GetViewMatrix() const noexcept;
    // Get reverse-Z projection matrix
    [[nodiscard]] XMMATRIX GetProjectionMatrix() const noexcept;
    [[nodiscard]] XMMATRIX GetViewProjectionMatrix() const noexcept;

    // World-space position (use for origin-rebasing entity positions)
    [[nodiscard]] XMVECTOR GetPosition() const noexcept { return XMLoadFloat3(&m_worldPosition); }
    [[nodiscard]] XMVECTOR GetForward() const noexcept { return XMLoadFloat3(&m_forward); }
    [[nodiscard]] XMVECTOR GetUp() const noexcept { return XMLoadFloat3(&m_up); }
    [[nodiscard]] XMVECTOR GetRight() const noexcept;

    [[nodiscard]] XMVECTOR GetLightDirection() const noexcept { return XMLoadFloat3(&m_lightDirection); }
    void SetLightDirection(FXMVECTOR dir) noexcept { XMStoreFloat3(&m_lightDirection, dir); }

    [[nodiscard]] float GetNearClip() const noexcept { return m_nearZ; }
    [[nodiscard]] float GetFarClip() const noexcept { return m_farZ; }
    [[nodiscard]] float GetFovY() const noexcept { return m_fovY; }
    [[nodiscard]] float GetAspect() const noexcept { return m_aspect; }
    [[nodiscard]] CameraMode GetMode() const noexcept { return m_mode; }

    // Frustum culling (origin-rebased space)
    [[nodiscard]] bool IsVisible(FXMVECTOR rebasedCenter, float radius) const noexcept;

    // Rebase a world position relative to the camera
    [[nodiscard]] XMVECTOR XM_CALLCONV RebasePosition(FXMVECTOR worldPos) const noexcept
    {
      return XMVectorSubtract(worldPos, GetPosition());
    }

    // Chase camera target
    void SetChaseTarget(FXMVECTOR targetPos, FXMVECTOR targetForward) noexcept;

    // Free-fly input (yaw/pitch in radians, moveDir in local space)
    void Rotate(float yaw, float pitch) noexcept;
    void Translate(FXMVECTOR localDirection, float distance) noexcept;

    // Tactical zoom
    void Zoom(float delta) noexcept;

  private:
    void RebuildViewMatrix() noexcept;
    void RebuildProjectionMatrix() noexcept;

    CameraMode m_mode = CameraMode::FreeFly;

    XMFLOAT3 m_worldPosition = { 0, 5, -20 };
    XMFLOAT3 m_forward = { 0, 0, 1 };
    XMFLOAT3 m_up = { 0, 1, 0 };
    XMFLOAT3 m_lightDirection = { 0.577f, -0.577f, 0.577f };

    // Chase camera state
    XMFLOAT3 m_chaseTargetPos{};
    XMFLOAT3 m_chaseTargetForward = { 0, 0, 1 };
    float m_chaseDistance = 30.0f;
    float m_chaseHeight = 10.0f;

    // Free-fly state
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;

    // Zoom
    float m_zoomLevel = 1.0f;
    static constexpr float MIN_ZOOM = 0.1f;
    static constexpr float MAX_ZOOM = 10.0f;

    // Projection parameters
    float m_fovY = XM_PIDIV4;
    float m_aspect = 16.0f / 9.0f;
    float m_nearZ = 0.5f;
    float m_farZ = 20000.0f;  // 20 km draw distance

    XMFLOAT4X4 m_viewMatrix;
    XMFLOAT4X4 m_projMatrix;
    BoundingFrustum m_frustum;

    bool m_viewDirty = true;
    bool m_projDirty = true;
  };
}
