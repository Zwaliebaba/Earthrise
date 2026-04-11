#pragma once

#include "GameTypes/EntityHandle.h"

struct ClientEntity;
class ClientWorldState;
namespace Neuron::Graphics { class Camera; class ConstantBufferAllocator; class SpriteBatch; }

// HUDBar — computed layout for a single horizontal status bar.
struct HUDBar
{
  RECT  Outline;       // Full-size outline in screen coords
  RECT  Fill;          // Filled portion in screen coords
  float FillFraction;  // 0..1 how full the bar is
};

// HUDLayout — pure-computation helper for HUD element positioning.
// All methods are static and unit-testable without GPU resources.
struct HUDLayout
{
  // Compute a horizontal bar given value/max and screen-space bounds.
  static HUDBar ComputeBar(float _value, float _max,
    LONG _left, LONG _top, LONG _width, LONG _height) noexcept;

  // Standard bar positions (anchored to bottom-left of screen).
  static HUDBar ShieldBar(float _hp, float _max, UINT _screenW, UINT _screenH) noexcept;
  static HUDBar ArmorBar(float _hp, float _max, UINT _screenW, UINT _screenH) noexcept;
  static HUDBar HullBar(float _hp, float _max, UINT _screenW, UINT _screenH) noexcept;
  static HUDBar EnergyBar(float _energy, float _max, UINT _screenW, UINT _screenH) noexcept;
  static HUDBar SpeedBar(float _speed, float _max, UINT _screenW, UINT _screenH) noexcept;

  // Target panel bars (anchored to top-right of screen).
  static HUDBar TargetShieldBar(float _hp, float _max, UINT _screenW, UINT _screenH) noexcept;
  static HUDBar TargetArmorBar(float _hp, float _max, UINT _screenW, UINT _screenH) noexcept;
  static HUDBar TargetHullBar(float _hp, float _max, UINT _screenW, UINT _screenH) noexcept;

  // Layout constants (pixels).
  static constexpr LONG BAR_WIDTH     = 200;
  static constexpr LONG BAR_HEIGHT    = 12;
  static constexpr LONG BAR_SPACING   = 4;
  static constexpr LONG MARGIN        = 20;
  static constexpr LONG TARGET_BAR_WIDTH = 160;
};

// HUD — renders the in-game overlay (ship status bars, speed indicator, target info).
// All rendering is done via SpriteBatch solid-color quads (Darwinia-style minimalist chrome).
class HUD
{
public:
  HUD() = default;

  void Initialize(ClientWorldState* _worldState);

  // Set the entity whose stats are shown (flagship or selected ship).
  void SetFocusEntity(Neuron::EntityHandle _handle) noexcept { m_focusEntity = _handle; }
  [[nodiscard]] Neuron::EntityHandle GetFocusEntity() const noexcept { return m_focusEntity; }

  // Set the current target (displayed in top-right target panel).
  void SetTarget(Neuron::EntityHandle _handle) noexcept { m_targetEntity = _handle; }
  [[nodiscard]] Neuron::EntityHandle GetTarget() const noexcept { return m_targetEntity; }

  // Render the HUD overlay.
  void Render(ID3D12GraphicsCommandList* _cmdList,
    Neuron::Graphics::ConstantBufferAllocator& _cbAlloc,
    Neuron::Graphics::SpriteBatch& _spriteBatch,
    UINT _screenWidth, UINT _screenHeight);

private:
  void RenderBar(Neuron::Graphics::SpriteBatch& _sb, const HUDBar& _bar,
    FXMVECTOR _fillColor, FXMVECTOR _outlineColor);

  void RenderPlayerBars(Neuron::Graphics::SpriteBatch& _sb,
    const ClientEntity& _entity, UINT _screenW, UINT _screenH);

  void RenderTargetPanel(Neuron::Graphics::SpriteBatch& _sb,
    const ClientEntity& _target, UINT _screenW, UINT _screenH);

  void RenderCrosshair(Neuron::Graphics::SpriteBatch& _sb,
    UINT _screenW, UINT _screenH);

  ClientWorldState* m_worldState = nullptr;
  Neuron::EntityHandle m_focusEntity;
  Neuron::EntityHandle m_targetEntity;
};
