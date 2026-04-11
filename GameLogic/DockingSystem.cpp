#include "pch.h"
#include "DockingSystem.h"

namespace GameLogic
{
  DockingSystem::DockingSystem(SpaceObjectManager& _manager)
    : m_manager(_manager)
  {
  }

  void DockingSystem::ProcessCollisions(const std::vector<CollisionEvent>& _collisions)
  {
    m_dockEvents.clear();

    for (const auto& col : _collisions)
    {
      Neuron::SpaceObject* objA = m_manager.GetSpaceObject(col.A);
      Neuron::SpaceObject* objB = m_manager.GetSpaceObject(col.B);
      if (!objA || !objB || !objA->Active || !objB->Active)
        continue;

      Neuron::SpaceObject* ship = nullptr;
      Neuron::SpaceObject* station = nullptr;

      if (objA->Category == Neuron::SpaceObjectCategory::Ship &&
          objB->Category == Neuron::SpaceObjectCategory::Station)
      {
        ship = objA;
        station = objB;
      }
      else if (objB->Category == Neuron::SpaceObjectCategory::Ship &&
               objA->Category == Neuron::SpaceObjectCategory::Station)
      {
        ship = objB;
        station = objA;
      }
      else
      {
        continue;
      }

      Neuron::StationData* stationData = m_manager.GetStationData(station->Handle);
      if (!stationData)
        continue;

      // Check docking radius (more precise than bounding radius collision).
      XMVECTOR shipPos = XMLoadFloat3(&ship->Position);
      XMVECTOR stationPos = XMLoadFloat3(&station->Position);
      float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(shipPos, stationPos)));

      if (dist <= stationData->DockingRadius)
      {
        m_dockEvents.push_back({ ship->Handle, station->Handle });
      }
    }
  }

  void DockingSystem::CheckDockingRange(Neuron::EntityHandle _ship, Neuron::EntityHandle _station)
  {
    Neuron::SpaceObject* shipObj = m_manager.GetSpaceObject(_ship);
    Neuron::SpaceObject* stationObj = m_manager.GetSpaceObject(_station);
    if (!shipObj || !stationObj || !shipObj->Active || !stationObj->Active)
      return;

    if (stationObj->Category != Neuron::SpaceObjectCategory::Station)
      return;

    Neuron::StationData* stationData = m_manager.GetStationData(_station);
    if (!stationData)
      return;

    XMVECTOR shipPos = XMLoadFloat3(&shipObj->Position);
    XMVECTOR stationPos = XMLoadFloat3(&stationObj->Position);
    float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(shipPos, stationPos)));

    if (dist <= stationData->DockingRadius)
    {
      m_dockEvents.push_back({ _ship, _station });
    }
  }
}
