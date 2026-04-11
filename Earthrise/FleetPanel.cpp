#include "pch.h"
#include "FleetPanel.h"
#include "ClientWorldState.h"
#include "SpriteBatch.h"

using namespace Neuron;
using namespace Neuron::Graphics;

void FleetPanel::Initialize(ClientWorldState* _worldState)
{
  m_worldState = _worldState;
}

void FleetPanel::SetFleet(const std::vector<EntityHandle>& _fleet)
{
  m_fleet = _fleet;
}

std::vector<FleetPanelEntry> FleetPanel::ComputeLayout(UINT /*_screenW*/, UINT /*_screenH*/) const
{
  std::vector<FleetPanelEntry> entries;
  entries.reserve(m_fleet.size());

  LONG y = PANEL_TOP;

  for (const auto& handle : m_fleet)
  {
    FleetPanelEntry entry;
    entry.Handle = handle;
    entry.SlotRect = { PANEL_LEFT, y, PANEL_LEFT + PANEL_WIDTH, y + SLOT_HEIGHT };

    // Compute bar fractions from world state
    if (m_worldState)
    {
      const ClientEntity* ent = m_worldState->GetEntity(handle);
      if (ent && ent->HasShipStats)
      {
        entry.HullFraction   = (ent->HullMaxHP > 0.0f) ? ent->HullHP / ent->HullMaxHP : 0.0f;
        entry.ShieldFraction = (ent->ShieldMaxHP > 0.0f) ? ent->ShieldHP / ent->ShieldMaxHP : 0.0f;
      }
    }

    // Clamp fractions
    entry.HullFraction   = (std::max)(0.0f, (std::min)(entry.HullFraction, 1.0f));
    entry.ShieldFraction = (std::max)(0.0f, (std::min)(entry.ShieldFraction, 1.0f));

    // Bar rects within the slot
    LONG barWidth = PANEL_WIDTH - BAR_MARGIN * 2;
    LONG shieldBarTop = y + BAR_MARGIN;
    LONG hullBarTop   = shieldBarTop + BAR_HEIGHT + 1;

    entry.ShieldBar = { PANEL_LEFT + BAR_MARGIN, shieldBarTop,
      PANEL_LEFT + BAR_MARGIN + static_cast<LONG>(static_cast<float>(barWidth) * entry.ShieldFraction),
      shieldBarTop + BAR_HEIGHT };

    entry.HullBar = { PANEL_LEFT + BAR_MARGIN, hullBarTop,
      PANEL_LEFT + BAR_MARGIN + static_cast<LONG>(static_cast<float>(barWidth) * entry.HullFraction),
      hullBarTop + BAR_HEIGHT };

    entries.push_back(entry);
    y += SLOT_HEIGHT;
  }

  return entries;
}

void FleetPanel::Render(ID3D12GraphicsCommandList* _cmdList,
  ConstantBufferAllocator& _cbAlloc,
  SpriteBatch& _spriteBatch,
  UINT _screenWidth, UINT _screenHeight)
{
  if (m_fleet.empty()) return;

  auto entries = ComputeLayout(_screenWidth, _screenHeight);
  if (entries.empty()) return;

  _spriteBatch.Begin(_cmdList, _cbAlloc, _screenWidth, _screenHeight);

  for (const auto& entry : entries)
  {
    // Slot background
    XMVECTORF32 bgColor = { 0.0f, 0.0f, 0.0f, 0.4f };
    _spriteBatch.DrawRect(entry.SlotRect, bgColor);

    // Shield bar (cyan)
    if (entry.ShieldFraction > 0.0f)
    {
      XMVECTORF32 shieldColor = { 0.0f, 0.8f, 1.0f, 0.7f };
      _spriteBatch.DrawRect(entry.ShieldBar, shieldColor);
    }

    // Hull bar (red-green gradient based on fraction)
    if (entry.HullFraction > 0.0f)
    {
      float r = 1.0f - entry.HullFraction;
      float g = entry.HullFraction;
      XMVECTORF32 hullColor = { r, g, 0.1f, 0.7f };
      _spriteBatch.DrawRect(entry.HullBar, hullColor);
    }

    // Slot border
    XMVECTORF32 borderColor = { 0.3f, 0.3f, 0.3f, 0.6f };
    const auto& sr = entry.SlotRect;
    RECT top = { sr.left, sr.top, sr.right, sr.top + 1 };
    RECT bot = { sr.left, sr.bottom - 1, sr.right, sr.bottom };
    RECT lft = { sr.left, sr.top, sr.left + 1, sr.bottom };
    RECT rgt = { sr.right - 1, sr.top, sr.right, sr.bottom };
    _spriteBatch.DrawRect(top, borderColor);
    _spriteBatch.DrawRect(bot, borderColor);
    _spriteBatch.DrawRect(lft, borderColor);
    _spriteBatch.DrawRect(rgt, borderColor);
  }

  _spriteBatch.End();
}
