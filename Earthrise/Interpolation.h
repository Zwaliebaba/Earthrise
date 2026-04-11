#pragma once

// Interpolation.h — timestamp-based entity interpolation utilities.
// Used by ClientWorldState to smoothly render entities between server ticks.

namespace Interpolation
{
  // Lerp between two positions at time fraction t ∈ [0, 1].
  [[nodiscard]] inline XMVECTOR XM_CALLCONV LerpPosition(
    FXMVECTOR _prev, FXMVECTOR _target, float _t) noexcept
  {
    return XMVectorLerp(_prev, _target, _t);
  }

  // Slerp between two orientations (quaternions) at time fraction t ∈ [0, 1].
  [[nodiscard]] inline XMVECTOR XM_CALLCONV SlerpOrientation(
    FXMVECTOR _prev, FXMVECTOR _target, float _t) noexcept
  {
    return XMQuaternionSlerp(_prev, _target, _t);
  }

  // Dead-reckoning extrapolation: position + velocity * dt.
  [[nodiscard]] inline XMVECTOR XM_CALLCONV Extrapolate(
    FXMVECTOR _position, FXMVECTOR _velocity, float _dt) noexcept
  {
    return XMVectorAdd(_position, XMVectorScale(_velocity, _dt));
  }

  // Compute interpolation alpha from elapsed time since last snapshot
  // and the snapshot interval. Clamped to a maximum to prevent runaway extrapolation.
  [[nodiscard]] inline float ComputeAlpha(
    float _elapsedSinceSnapshot, float _snapshotInterval,
    float _maxAlpha = 2.0f) noexcept
  {
    if (_snapshotInterval <= 0.0f)
      return 1.0f;
    float alpha = _elapsedSinceSnapshot / _snapshotInterval;
    return (alpha < _maxAlpha) ? alpha : _maxAlpha;
  }

  // Smooth interpolation with velocity-based extrapolation beyond alpha=1.
  // For alpha ∈ [0, 1]: lerp from prev to target.
  // For alpha ∈ (1, maxAlpha]: extrapolate from target using velocity.
  struct InterpolatedState
  {
    XMFLOAT3 Position;
    XMFLOAT4 Orientation;
  };

  [[nodiscard]] inline InterpolatedState XM_CALLCONV Interpolate(
    FXMVECTOR _prevPos, FXMVECTOR _targetPos,
    GXMVECTOR _prevOri, HXMVECTOR _targetOri,
    CXMVECTOR _velocity, float _alpha,
    float _snapshotInterval) noexcept
  {
    InterpolatedState result;

    float t = (_alpha < 1.0f) ? _alpha : 1.0f;

    // Position
    XMVECTOR pos = XMVectorLerp(_prevPos, _targetPos, t);
    if (_alpha > 1.0f)
    {
      float overshoot = (_alpha - 1.0f) * _snapshotInterval;
      pos = XMVectorAdd(_targetPos, XMVectorScale(_velocity, overshoot));
    }
    XMStoreFloat3(&result.Position, pos);

    // Orientation (slerp, clamped to [0, 1])
    XMVECTOR ori = XMQuaternionSlerp(_prevOri, _targetOri, t);
    XMStoreFloat4(&result.Orientation, ori);

    return result;
  }
}
