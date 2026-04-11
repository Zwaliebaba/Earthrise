#include "pch.h"
#include "JumpgateSystem.h"

namespace GameLogic
{
  JumpgateSystem::JumpgateSystem(SpaceObjectManager& _manager)
    : m_manager(_manager)
  {
  }

  void JumpgateSystem::RequestWarp(Neuron::EntityHandle _gateHandle,
                                   const std::vector<Neuron::EntityHandle>& _fleet)
  {
    m_pendingWarps.push_back({ _gateHandle, _fleet });
  }

  void JumpgateSystem::Update()
  {
    m_completedWarps.clear();

    for (auto it = m_pendingWarps.begin(); it != m_pendingWarps.end();)
    {
      auto& req = *it;

      Neuron::SpaceObject* gateObj = m_manager.GetSpaceObject(req.GateHandle);
      if (!gateObj || gateObj->Category != Neuron::SpaceObjectCategory::Jumpgate)
      {
        it = m_pendingWarps.erase(it);
        continue;
      }

      Neuron::JumpgateData* gateData = m_manager.GetJumpgateData(req.GateHandle);
      if (!gateData || !gateData->IsActive)
      {
        it = m_pendingWarps.erase(it);
        continue;
      }

      // Check if any fleet ship is within activation radius
      XMVECTOR gatePos = XMLoadFloat3(&gateObj->Position);
      float radiusSq = gateData->ActivationRadius * gateData->ActivationRadius;
      bool anyInRange = false;

      for (auto shipHandle : req.Fleet)
      {
        Neuron::SpaceObject* shipObj = m_manager.GetSpaceObject(shipHandle);
        if (!shipObj || !shipObj->Active)
          continue;

        XMVECTOR shipPos = XMLoadFloat3(&shipObj->Position);
        float distSq = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(shipPos, gatePos)));
        if (distSq <= radiusSq)
        {
          anyInRange = true;
          break;
        }
      }

      if (anyInRange)
      {
        // Teleport all fleet ships to destination position
        XMFLOAT3 dest = gateData->DestinationPosition;
        float offset = 0.0f;
        for (auto shipHandle : req.Fleet)
        {
          Neuron::SpaceObject* shipObj = m_manager.GetSpaceObject(shipHandle);
          if (!shipObj || !shipObj->Active)
            continue;

          // Spread ships slightly around the destination
          shipObj->Position = { dest.x + offset, dest.y, dest.z };
          shipObj->Velocity = { 0.0f, 0.0f, 0.0f };
          offset += 20.0f; // 20m spacing between fleet ships
        }

        m_completedWarps.push_back(std::move(req));
        it = m_pendingWarps.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }
}
