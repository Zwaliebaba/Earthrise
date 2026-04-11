#pragma once

#include "GameTypes/EntityHandle.h"

struct ClientEntity;
class ClientWorldState;
namespace Neuron::Graphics { class ConstantBufferAllocator; class SpriteBatch; }

// FleetPanelEntry — layout data for a single ship in the fleet panel.
struct FleetPanelEntry
{
  Neuron::EntityHandle Handle;
  float HullFraction  = 0.0f;
  float ShieldFraction = 0.0f;
  RECT  HullBar;
  RECT  ShieldBar;
  RECT  SlotRect;      // Full row rectangle for this ship
};

// FleetPanel — fleet command panel rendering and data binding.
// Displays a list of fleet ships with HP/shield bars on the left side of the screen.
// Pure layout computation is unit-testable.
class FleetPanel
{
public:
  FleetPanel() = default;

  void Initialize(ClientWorldState* _worldState);

  // Set the list of fleet ship handles to display.
  void SetFleet(const std::vector<Neuron::EntityHandle>& _fleet);

  // Compute layout entries from current fleet and world state.
  // Returns the entry list for unit-test inspection.
  [[nodiscard]] std::vector<FleetPanelEntry> ComputeLayout(UINT _screenW, UINT _screenH) const;

  // Render the fleet panel.
  void Render(ID3D12GraphicsCommandList* _cmdList,
    Neuron::Graphics::ConstantBufferAllocator& _cbAlloc,
    Neuron::Graphics::SpriteBatch& _spriteBatch,
    UINT _screenWidth, UINT _screenHeight);

  // Fleet ship count.
  [[nodiscard]] size_t ShipCount() const noexcept { return m_fleet.size(); }

  // Layout constants.
  static constexpr LONG PANEL_WIDTH   = 160;
  static constexpr LONG SLOT_HEIGHT   = 24;
  static constexpr LONG BAR_HEIGHT    = 6;
  static constexpr LONG BAR_MARGIN    = 2;
  static constexpr LONG PANEL_TOP     = 100;
  static constexpr LONG PANEL_LEFT    = 10;

private:
  ClientWorldState* m_worldState = nullptr;
  std::vector<Neuron::EntityHandle> m_fleet;
};
