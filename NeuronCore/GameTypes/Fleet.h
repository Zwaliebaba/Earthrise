#pragma once

#include "EntityHandle.h"

namespace Neuron
{
  static constexpr uint32_t MAX_FLEET_SHIPS = 20;

  // Formation type for fleet movement.
  enum class FormationType : uint8_t
  {
    None,       // No formation — each ship steers independently
    Line,       // Ships form a line behind the leader
    Wedge,      // V-shaped formation
    Sphere,     // Ships spread in a sphere around the center

    COUNT
  };

  // Fleet — a group of ships under one player's command.
  struct Fleet
  {
    uint32_t     SessionId  = 0;
    FormationType Formation = FormationType::None;

    EntityHandle Ships[MAX_FLEET_SHIPS] = {};
    uint8_t      ShipCount  = 0;

    // Add a ship to the fleet. Returns false if full.
    bool AddShip(EntityHandle _handle) noexcept
    {
      if (ShipCount >= MAX_FLEET_SHIPS)
        return false;
      Ships[ShipCount++] = _handle;
      return true;
    }

    // Remove a ship by handle. Returns false if not found.
    bool RemoveShip(EntityHandle _handle) noexcept
    {
      for (uint8_t i = 0; i < ShipCount; ++i)
      {
        if (Ships[i] == _handle)
        {
          Ships[i] = Ships[--ShipCount];
          Ships[ShipCount] = EntityHandle::Invalid();
          return true;
        }
      }
      return false;
    }

    // Check if a ship is in the fleet.
    [[nodiscard]] bool ContainsShip(EntityHandle _handle) const noexcept
    {
      for (uint8_t i = 0; i < ShipCount; ++i)
      {
        if (Ships[i] == _handle)
          return true;
      }
      return false;
    }

    // Get formation offset for the Nth ship in the fleet (relative to leader position).
    [[nodiscard]] static XMFLOAT3 GetFormationOffset(FormationType _type, uint8_t _index, uint8_t _total) noexcept
    {
      if (_index == 0 || _total <= 1)
        return { 0.0f, 0.0f, 0.0f }; // Leader stays at center

      float spacing = 8.0f; // Distance between ships

      switch (_type)
      {
      case FormationType::Line:
      {
        return { 0.0f, 0.0f, -spacing * static_cast<float>(_index) };
      }
      case FormationType::Wedge:
      {
        float side = (_index % 2 == 0) ? 1.0f : -1.0f;
        float rank = static_cast<float>((_index + 1) / 2);
        return { side * rank * spacing, 0.0f, -rank * spacing };
      }
      case FormationType::Sphere:
      {
        // Distribute on a sphere surface using golden ratio
        float phi = std::acos(1.0f - 2.0f * static_cast<float>(_index) / static_cast<float>(_total));
        float theta = 3.14159265f * (1.0f + std::sqrt(5.0f)) * static_cast<float>(_index);
        float r = spacing * 2.0f;
        return {
          r * std::sin(phi) * std::cos(theta),
          r * std::sin(phi) * std::sin(theta),
          r * std::cos(phi)
        };
      }
      default:
        return { 0.0f, 0.0f, 0.0f };
      }
    }
  };
}
