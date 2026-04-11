#include "pch.h"
#include "HUD.h"
#include "ClientWorldState.h"
#include "SpriteBatch.h"

using namespace Neuron;
using namespace Neuron::Graphics;

// ───── HUDLayout computation (pure, no GPU) ───────────────────────────────

HUDBar HUDLayout::ComputeBar(float _value, float _max,
  LONG _left, LONG _top, LONG _width, LONG _height) noexcept
{
  HUDBar bar{};
  bar.Outline = { _left, _top, _left + _width, _top + _height };
  bar.FillFraction = (_max > 0.0f) ? (std::max)(0.0f, (std::min)(_value / _max, 1.0f)) : 0.0f;

  LONG fillWidth = static_cast<LONG>(static_cast<float>(_width) * bar.FillFraction);
  bar.Fill = { _left, _top, _left + fillWidth, _top + _height };
  return bar;
}

HUDBar HUDLayout::ShieldBar(float _hp, float _max, UINT _screenW, UINT _screenH) noexcept
{
  (void)_screenW;
  LONG left = MARGIN;
  LONG top  = static_cast<LONG>(_screenH) - MARGIN - (BAR_HEIGHT + BAR_SPACING) * 5;
  return ComputeBar(_hp, _max, left, top, BAR_WIDTH, BAR_HEIGHT);
}

HUDBar HUDLayout::ArmorBar(float _hp, float _max, UINT _screenW, UINT _screenH) noexcept
{
  LONG left = MARGIN;
  LONG top  = static_cast<LONG>(_screenH) - MARGIN - (BAR_HEIGHT + BAR_SPACING) * 4;
  (void)_screenW;
  return ComputeBar(_hp, _max, left, top, BAR_WIDTH, BAR_HEIGHT);
}

HUDBar HUDLayout::HullBar(float _hp, float _max, UINT _screenW, UINT _screenH) noexcept
{
  LONG left = MARGIN;
  LONG top  = static_cast<LONG>(_screenH) - MARGIN - (BAR_HEIGHT + BAR_SPACING) * 3;
  (void)_screenW;
  return ComputeBar(_hp, _max, left, top, BAR_WIDTH, BAR_HEIGHT);
}

HUDBar HUDLayout::EnergyBar(float _energy, float _max, UINT _screenW, UINT _screenH) noexcept
{
  LONG left = MARGIN;
  LONG top  = static_cast<LONG>(_screenH) - MARGIN - (BAR_HEIGHT + BAR_SPACING) * 2;
  (void)_screenW;
  return ComputeBar(_energy, _max, left, top, BAR_WIDTH, BAR_HEIGHT);
}

HUDBar HUDLayout::SpeedBar(float _speed, float _max, UINT _screenW, UINT _screenH) noexcept
{
  LONG left = MARGIN;
  LONG top  = static_cast<LONG>(_screenH) - MARGIN - (BAR_HEIGHT + BAR_SPACING) * 1;
  (void)_screenW;
  return ComputeBar(_speed, _max, left, top, BAR_WIDTH, BAR_HEIGHT);
}

HUDBar HUDLayout::TargetShieldBar(float _hp, float _max, UINT _screenW, UINT /*_screenH*/) noexcept
{
  LONG left = static_cast<LONG>(_screenW) - MARGIN - TARGET_BAR_WIDTH;
  LONG top  = MARGIN;
  return ComputeBar(_hp, _max, left, top, TARGET_BAR_WIDTH, BAR_HEIGHT);
}

HUDBar HUDLayout::TargetArmorBar(float _hp, float _max, UINT _screenW, UINT /*_screenH*/) noexcept
{
  LONG left = static_cast<LONG>(_screenW) - MARGIN - TARGET_BAR_WIDTH;
  LONG top  = MARGIN + (BAR_HEIGHT + BAR_SPACING) * 1;
  return ComputeBar(_hp, _max, left, top, TARGET_BAR_WIDTH, BAR_HEIGHT);
}

HUDBar HUDLayout::TargetHullBar(float _hp, float _max, UINT _screenW, UINT /*_screenH*/) noexcept
{
  LONG left = static_cast<LONG>(_screenW) - MARGIN - TARGET_BAR_WIDTH;
  LONG top  = MARGIN + (BAR_HEIGHT + BAR_SPACING) * 2;
  return ComputeBar(_hp, _max, left, top, TARGET_BAR_WIDTH, BAR_HEIGHT);
}

// ───── HUD rendering ──────────────────────────────────────────────────────

void HUD::Initialize(ClientWorldState* _worldState)
{
  m_worldState = _worldState;
}

void HUD::RenderBar(SpriteBatch& _sb, const HUDBar& _bar,
  FXMVECTOR _fillColor, FXMVECTOR _outlineColor)
{
  // Background (dark)
  XMVECTORF32 bgColor = { 0.0f, 0.0f, 0.0f, 0.5f };
  _sb.DrawRect(_bar.Outline, bgColor);

  // Fill
  if (_bar.FillFraction > 0.0f)
    _sb.DrawRect(_bar.Fill, _fillColor);

  // Outline border (1px)
  const auto& r = _bar.Outline;
  RECT top    = { r.left, r.top, r.right, r.top + 1 };
  RECT bot    = { r.left, r.bottom - 1, r.right, r.bottom };
  RECT lft    = { r.left, r.top, r.left + 1, r.bottom };
  RECT rgt    = { r.right - 1, r.top, r.right, r.bottom };
  _sb.DrawRect(top, _outlineColor);
  _sb.DrawRect(bot, _outlineColor);
  _sb.DrawRect(lft, _outlineColor);
  _sb.DrawRect(rgt, _outlineColor);
}

void HUD::RenderPlayerBars(SpriteBatch& _sb,
  const ClientEntity& _entity, UINT _screenW, UINT _screenH)
{
  XMVECTORF32 outlineColor = { 0.4f, 0.4f, 0.4f, 0.8f };

  // Shield — cyan
  {
    auto bar = HUDLayout::ShieldBar(_entity.ShieldHP, _entity.ShieldMaxHP, _screenW, _screenH);
    XMVECTORF32 fillColor = { 0.0f, 0.8f, 1.0f, 0.9f };
    RenderBar(_sb, bar, fillColor, outlineColor);
  }
  // Armor — yellow
  {
    auto bar = HUDLayout::ArmorBar(_entity.ArmorHP, _entity.ArmorMaxHP, _screenW, _screenH);
    XMVECTORF32 fillColor = { 1.0f, 0.8f, 0.0f, 0.9f };
    RenderBar(_sb, bar, fillColor, outlineColor);
  }
  // Hull — red
  {
    auto bar = HUDLayout::HullBar(_entity.HullHP, _entity.HullMaxHP, _screenW, _screenH);
    XMVECTORF32 fillColor = { 1.0f, 0.2f, 0.1f, 0.9f };
    RenderBar(_sb, bar, fillColor, outlineColor);
  }
  // Energy — purple
  {
    auto bar = HUDLayout::EnergyBar(_entity.Energy, _entity.EnergyMax, _screenW, _screenH);
    XMVECTORF32 fillColor = { 0.6f, 0.2f, 1.0f, 0.9f };
    RenderBar(_sb, bar, fillColor, outlineColor);
  }
  // Speed — green
  {
    auto bar = HUDLayout::SpeedBar(_entity.Speed, _entity.MaxSpeed, _screenW, _screenH);
    XMVECTORF32 fillColor = { 0.0f, 1.0f, 0.3f, 0.9f };
    RenderBar(_sb, bar, fillColor, outlineColor);
  }
}

void HUD::RenderTargetPanel(SpriteBatch& _sb,
  const ClientEntity& _target, UINT _screenW, UINT _screenH)
{
  XMVECTORF32 outlineColor = { 0.6f, 0.2f, 0.2f, 0.8f };

  // Shield
  {
    auto bar = HUDLayout::TargetShieldBar(_target.ShieldHP, _target.ShieldMaxHP, _screenW, _screenH);
    XMVECTORF32 fillColor = { 0.0f, 0.8f, 1.0f, 0.7f };
    RenderBar(_sb, bar, fillColor, outlineColor);
  }
  // Armor
  {
    auto bar = HUDLayout::TargetArmorBar(_target.ArmorHP, _target.ArmorMaxHP, _screenW, _screenH);
    XMVECTORF32 fillColor = { 1.0f, 0.8f, 0.0f, 0.7f };
    RenderBar(_sb, bar, fillColor, outlineColor);
  }
  // Hull
  {
    auto bar = HUDLayout::TargetHullBar(_target.HullHP, _target.HullMaxHP, _screenW, _screenH);
    XMVECTORF32 fillColor = { 1.0f, 0.2f, 0.1f, 0.7f };
    RenderBar(_sb, bar, fillColor, outlineColor);
  }
}

void HUD::RenderCrosshair(SpriteBatch& _sb, UINT _screenW, UINT _screenH)
{
  LONG cx = static_cast<LONG>(_screenW / 2);
  LONG cy = static_cast<LONG>(_screenH / 2);
  constexpr LONG SIZE = 8;
  constexpr LONG GAP  = 3;
  XMVECTORF32 color = { 0.0f, 1.0f, 0.0f, 0.6f };

  // Horizontal lines
  RECT left  = { cx - SIZE - GAP, cy, cx - GAP, cy + 1 };
  RECT right = { cx + GAP, cy, cx + SIZE + GAP, cy + 1 };
  // Vertical lines
  RECT top   = { cx, cy - SIZE - GAP, cx + 1, cy - GAP };
  RECT bot   = { cx, cy + GAP, cx + 1, cy + SIZE + GAP };

  _sb.DrawRect(left, color);
  _sb.DrawRect(right, color);
  _sb.DrawRect(top, color);
  _sb.DrawRect(bot, color);
}

void HUD::Render(ID3D12GraphicsCommandList* _cmdList,
  ConstantBufferAllocator& _cbAlloc,
  SpriteBatch& _spriteBatch,
  UINT _screenWidth, UINT _screenHeight)
{
  if (!m_worldState) return;

  _spriteBatch.Begin(_cmdList, _cbAlloc, _screenWidth, _screenHeight);

  // Crosshair
  RenderCrosshair(_spriteBatch, _screenWidth, _screenHeight);

  // Player ship status bars
  const ClientEntity* focus = m_worldState->GetEntity(m_focusEntity);
  if (focus && focus->HasShipStats)
    RenderPlayerBars(_spriteBatch, *focus, _screenWidth, _screenHeight);

  // Target panel
  const ClientEntity* target = m_worldState->GetEntity(m_targetEntity);
  if (target && target->HasShipStats)
    RenderTargetPanel(_spriteBatch, *target, _screenWidth, _screenHeight);

  _spriteBatch.End();
}
