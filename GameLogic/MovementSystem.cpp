#include "pch.h"
#include "MovementSystem.h"

namespace GameLogic
{
  MovementSystem::MovementSystem(SpaceObjectManager& _manager)
    : m_manager(_manager)
  {
    m_commands.resize(MAX_ENTITIES);
  }

  void MovementSystem::SetMoveTarget(Neuron::EntityHandle _handle, const XMFLOAT3& _target)
  {
    if (!m_manager.IsValid(_handle))
      return;

    uint32_t idx = _handle.Index() - 1;
    m_commands[idx].HasTarget = true;
    m_commands[idx].TargetPosition = _target;
  }

  void MovementSystem::ClearMoveTarget(Neuron::EntityHandle _handle)
  {
    if (!m_manager.IsValid(_handle))
      return;

    uint32_t idx = _handle.Index() - 1;
    m_commands[idx] = MovementCommand{};
  }

  bool MovementSystem::HasMoveTarget(Neuron::EntityHandle _handle) const
  {
    if (!m_manager.IsValid(_handle))
      return false;

    uint32_t idx = _handle.Index() - 1;
    return m_commands[idx].HasTarget;
  }

  void MovementSystem::Update(float _deltaTime)
  {
    // Update ships with steering/arrival
    m_manager.ForEachCategory(Neuron::SpaceObjectCategory::Ship, [&](Neuron::SpaceObject& _obj)
    {
      Neuron::ShipData* ship = m_manager.GetShipData(_obj.Handle);
      if (ship)
        UpdateShip(_obj, *ship, _deltaTime);
    });

    // Update projectiles (straight-line or homing)
    m_manager.ForEachCategory(Neuron::SpaceObjectCategory::Projectile, [&](Neuron::SpaceObject& _obj)
    {
      Neuron::ProjectileData* proj = m_manager.GetProjectileData(_obj.Handle);
      if (proj)
        UpdateProjectile(_obj, *proj, _deltaTime);
    });

    // Integrate velocity for all active entities (position += velocity * dt)
    m_manager.ForEachActive([&](Neuron::SpaceObject& _obj)
    {
      XMVECTOR pos = XMLoadFloat3(&_obj.Position);
      XMVECTOR vel = XMLoadFloat3(&_obj.Velocity);
      pos = XMVectorAdd(pos, XMVectorScale(vel, _deltaTime));
      XMStoreFloat3(&_obj.Position, pos);
    });
  }

  void MovementSystem::UpdateShip(Neuron::SpaceObject& _obj, Neuron::ShipData& _ship, float _dt)
  {
    uint32_t idx = _obj.Handle.Index() - 1;
    MovementCommand& cmd = m_commands[idx];

    if (!cmd.HasTarget)
    {
      // No target: apply drag to slow down
      XMVECTOR vel = XMLoadFloat3(&_obj.Velocity);
      float speed = XMVectorGetX(XMVector3Length(vel));
      if (speed > 0.1f)
      {
        float newSpeed = std::max(0.0f, speed - _ship.Thrust * _dt);
        vel = XMVectorScale(XMVector3Normalize(vel), newSpeed);
        XMStoreFloat3(&_obj.Velocity, vel);
      }
      else
      {
        _obj.Velocity = { 0.0f, 0.0f, 0.0f };
      }
      return;
    }

    XMVECTOR pos = XMLoadFloat3(&_obj.Position);
    XMVECTOR target = XMLoadFloat3(&cmd.TargetPosition);
    XMVECTOR toTarget = XMVectorSubtract(target, pos);
    float distance = XMVectorGetX(XMVector3Length(toTarget));

    // Arrived?
    if (distance < cmd.ArrivalThreshold)
    {
      cmd.HasTarget = false;
      _obj.Velocity = { 0.0f, 0.0f, 0.0f };
      return;
    }

    XMVECTOR desiredDir = XMVector3Normalize(toTarget);

    // Rotate orientation toward desired direction (quaternion slerp)
    XMVECTOR currentQuat = XMLoadFloat4(&_obj.Orientation);
    XMVECTOR forward = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), currentQuat);

    float dot = XMVectorGetX(XMVector3Dot(forward, desiredDir));
    dot = std::clamp(dot, -1.0f, 1.0f);

    if (dot < 0.9999f)
    {
      // Compute rotation from current forward to desired direction
      XMVECTOR axis = XMVector3Cross(forward, desiredDir);
      float axisLen = XMVectorGetX(XMVector3Length(axis));

      if (axisLen > 1e-6f)
      {
        axis = XMVectorScale(axis, 1.0f / axisLen);
        float angle = std::acos(dot);
        float maxAngle = _ship.TurnRate * _dt;
        angle = std::min(angle, maxAngle);

        XMVECTOR deltaQuat = XMQuaternionRotationAxis(axis, angle);
        currentQuat = XMQuaternionNormalize(XMQuaternionMultiply(currentQuat, deltaQuat));
        XMStoreFloat4(&_obj.Orientation, currentQuat);
      }
    }

    // Recompute forward after rotation
    forward = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), currentQuat);

    // Arrival behavior: decelerate when close
    float arrivalSpeed = _ship.MaxSpeed;
    float slowRadius = _ship.MaxSpeed * _ship.MaxSpeed / (2.0f * _ship.Thrust);
    if (distance < slowRadius)
    {
      arrivalSpeed = _ship.MaxSpeed * (distance / slowRadius);
    }

    // Accelerate toward desired speed along forward direction
    XMVECTOR vel = XMLoadFloat3(&_obj.Velocity);
    float currentSpeed = XMVectorGetX(XMVector3Length(vel));
    float targetSpeed = std::min(arrivalSpeed, _ship.MaxSpeed);

    if (currentSpeed < targetSpeed)
    {
      float newSpeed = std::min(currentSpeed + _ship.Thrust * _dt, targetSpeed);
      vel = XMVectorScale(forward, newSpeed);
    }
    else
    {
      vel = XMVectorScale(forward, targetSpeed);
    }

    XMStoreFloat3(&_obj.Velocity, vel);
  }

  void MovementSystem::UpdateProjectile(Neuron::SpaceObject& _obj, Neuron::ProjectileData& _proj, float _dt)
  {
    // Decrement lifetime
    _proj.Lifetime -= _dt;

    if (_proj.TurnRate > 0.0f && _proj.Target.IsValid())
    {
      // Homing: steer toward target
      Neuron::SpaceObject* targetObj = m_manager.GetSpaceObject(_proj.Target);
      if (targetObj && targetObj->Active)
      {
        XMVECTOR pos = XMLoadFloat3(&_obj.Position);
        XMVECTOR targetPos = XMLoadFloat3(&targetObj->Position);
        XMVECTOR toTarget = XMVector3Normalize(XMVectorSubtract(targetPos, pos));

        XMVECTOR currentDir = XMVector3Normalize(XMLoadFloat3(&_obj.Velocity));
        float dotP = XMVectorGetX(XMVector3Dot(currentDir, toTarget));
        dotP = std::clamp(dotP, -1.0f, 1.0f);

        if (dotP < 0.9999f)
        {
          XMVECTOR axis = XMVector3Cross(currentDir, toTarget);
          float axisLen = XMVectorGetX(XMVector3Length(axis));
          if (axisLen > 1e-6f)
          {
            axis = XMVectorScale(axis, 1.0f / axisLen);
            float angle = std::acos(dotP);
            float maxAngle = _proj.TurnRate * _dt;
            angle = std::min(angle, maxAngle);

            XMVECTOR rot = XMQuaternionRotationAxis(axis, angle);
            XMVECTOR newDir = XMVector3Rotate(currentDir, rot);
            XMStoreFloat3(&_obj.Velocity, XMVectorScale(newDir, _proj.Speed));
          }
        }
      }
    }
    // Non-homing projectiles just keep their velocity (set at spawn).
  }
}
