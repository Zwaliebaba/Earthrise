#include "pch.h"
#include "CombatSystem.h"
#include "CollisionSystem.h"

namespace GameLogic
{
  CombatSystem::CombatSystem(SpaceObjectManager& _manager, MovementSystem& _movement)
    : m_manager(_manager)
    , m_movement(_movement)
  {
    m_attackStates.resize(MAX_ENTITIES);
  }

  void CombatSystem::SetAttackTarget(Neuron::EntityHandle _ship, Neuron::EntityHandle _target)
  {
    if (!m_manager.IsValid(_ship))
      return;
    uint32_t idx = _ship.Index() - 1;
    m_attackStates[idx].Target = _target;
  }

  void CombatSystem::ClearAttackTarget(Neuron::EntityHandle _ship)
  {
    if (!m_manager.IsValid(_ship))
      return;
    uint32_t idx = _ship.Index() - 1;
    m_attackStates[idx].Target = Neuron::EntityHandle::Invalid();
  }

  Neuron::EntityHandle CombatSystem::GetAttackTarget(Neuron::EntityHandle _ship) const
  {
    if (!m_manager.IsValid(_ship))
      return Neuron::EntityHandle::Invalid();
    uint32_t idx = _ship.Index() - 1;
    return m_attackStates[idx].Target;
  }

  void CombatSystem::Update(float _deltaTime)
  {
    // For each ship with an attack target, update turrets and fire.
    m_manager.ForEachCategory(Neuron::SpaceObjectCategory::Ship, [&](Neuron::SpaceObject& _obj)
    {
      uint32_t idx = _obj.Handle.Index() - 1;
      auto& attackState = m_attackStates[idx];

      if (!attackState.Target.IsValid())
        return;

      // Validate target still exists.
      Neuron::SpaceObject* targetObj = m_manager.GetSpaceObject(attackState.Target);
      if (!targetObj || !targetObj->Active)
      {
        attackState.Target = Neuron::EntityHandle::Invalid();
        return;
      }

      Neuron::ShipData* ship = m_manager.GetShipData(_obj.Handle);
      if (!ship)
        return;

      // Move toward the target (chase behavior).
      // Set move target to the target's position so MovementSystem handles steering.
      m_movement.SetMoveTarget(_obj.Handle, targetObj->Position);

      XMVECTOR shipPos = XMLoadFloat3(&_obj.Position);
      XMVECTOR targetPos = XMLoadFloat3(&targetObj->Position);
      float distanceSq = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(shipPos, targetPos)));

      // Update each mounted turret.
      for (uint8_t t = 0; t < ship->TurretSlotCount; ++t)
      {
        Neuron::EntityHandle turretHandle = ship->TurretSlots[t];
        if (!turretHandle.IsValid() || !m_manager.IsValid(turretHandle))
          continue;

        Neuron::TurretData* turret = m_manager.GetTurretData(turretHandle);
        if (!turret)
          continue;

        // Tick cooldown.
        if (turret->CooldownTimer > 0.0f)
        {
          turret->CooldownTimer -= _deltaTime;
          continue;
        }

        // Check range.
        float rangeSq = turret->Range * turret->Range;
        if (distanceSq > rangeSq)
          continue;

        // Fire! Spawn projectile and reset cooldown.
        turret->Target = attackState.Target;
        turret->CooldownTimer = (turret->FireRate > 0.0f) ? (1.0f / turret->FireRate) : 1.0f;

        SpawnProjectile(turretHandle, attackState.Target, *turret, _obj.Position);
      }
    });
  }

  void CombatSystem::SpawnProjectile(Neuron::EntityHandle /*_turret*/, Neuron::EntityHandle _target,
    const Neuron::TurretData& _turretData, const XMFLOAT3& _spawnPos)
  {
    // Create a projectile entity at the ship's position heading toward the target.
    auto projHandle = m_manager.CreateEntity(
      Neuron::SpaceObjectCategory::Projectile,
      _spawnPos,
      0, // No mesh hash for now — could be set from turret's projectile def
      0);

    if (!projHandle.IsValid())
      return;

    Neuron::SpaceObject* projObj = m_manager.GetSpaceObject(projHandle);
    Neuron::ProjectileData* projData = m_manager.GetProjectileData(projHandle);
    if (!projObj || !projData)
      return;

    // Set projectile data from turret stats.
    projData->Source = _turretData.ParentShip;
    projData->Target = _target;
    projData->Damage = _turretData.Damage;
    projData->Speed = _turretData.ProjectileSpeed;
    projData->Lifetime = 5.0f; // Default 5-second lifetime
    projData->TurnRate = 1.0f; // Slight homing by default

    // Set initial velocity toward target.
    Neuron::SpaceObject* targetObj = m_manager.GetSpaceObject(_target);
    if (targetObj && targetObj->Active)
    {
      XMVECTOR spawnPosV = XMLoadFloat3(&_spawnPos);
      XMVECTOR targetPosV = XMLoadFloat3(&targetObj->Position);
      XMVECTOR dir = XMVector3Normalize(XMVectorSubtract(targetPosV, spawnPosV));
      XMVECTOR vel = XMVectorScale(dir, _turretData.ProjectileSpeed);
      XMStoreFloat3(&projObj->Velocity, vel);
    }
    else
    {
      // Fire forward if target is gone
      projObj->Velocity = { 0.0f, 0.0f, _turretData.ProjectileSpeed };
    }

    projObj->BoundingRadius = 0.5f;
    projObj->Color = { 1.0f, 0.3f, 0.1f, 1.0f }; // Orange-red projectile color
  }

  void CombatSystem::ProcessCollisions(const std::vector<CollisionEvent>& _collisions)
  {
    m_destroyed.clear();
    m_projectileHits.clear();

    for (const auto& col : _collisions)
    {
      Neuron::SpaceObject* objA = m_manager.GetSpaceObject(col.A);
      Neuron::SpaceObject* objB = m_manager.GetSpaceObject(col.B);
      if (!objA || !objB || !objA->Active || !objB->Active)
        continue;

      // Projectile-Ship collision
      Neuron::SpaceObject* projectile = nullptr;
      Neuron::SpaceObject* ship = nullptr;

      if (objA->Category == Neuron::SpaceObjectCategory::Projectile &&
          objB->Category == Neuron::SpaceObjectCategory::Ship)
      {
        projectile = objA;
        ship = objB;
      }
      else if (objB->Category == Neuron::SpaceObjectCategory::Projectile &&
               objA->Category == Neuron::SpaceObjectCategory::Ship)
      {
        projectile = objB;
        ship = objA;
      }
      else
      {
        continue; // Only handle projectile-ship collisions in combat.
      }

      Neuron::ProjectileData* projData = m_manager.GetProjectileData(projectile->Handle);
      Neuron::ShipData* shipData = m_manager.GetShipData(ship->Handle);
      if (!projData || !shipData)
        continue;

      // Don't damage the ship that fired the projectile.
      if (projData->Source == ship->Handle)
        continue;

      // Apply damage.
      bool destroyed = ApplyDamage(*shipData, projData->Damage);
      m_projectileHits.push_back(projectile->Handle);

      if (destroyed)
      {
        m_destroyed.push_back({ ship->Handle, ship->Position });
      }
    }
  }

  bool CombatSystem::ApplyDamage(Neuron::ShipData& _ship, float _damage)
  {
    float remaining = _damage;

    // Shields absorb first.
    if (_ship.ShieldHP > 0.0f)
    {
      float absorbed = std::min(_ship.ShieldHP, remaining);
      _ship.ShieldHP -= absorbed;
      remaining -= absorbed;
    }

    // Then armor.
    if (remaining > 0.0f && _ship.ArmorHP > 0.0f)
    {
      float absorbed = std::min(_ship.ArmorHP, remaining);
      _ship.ArmorHP -= absorbed;
      remaining -= absorbed;
    }

    // Then hull.
    if (remaining > 0.0f)
    {
      _ship.HullHP -= remaining;
    }

    return _ship.HullHP <= 0.0f;
  }
}
