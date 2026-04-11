#include "pch.h"
#include "JumpgateUI.h"
#include "ClientWorldState.h"
#include "Camera.h"
#include "SpriteBatch.h"

using namespace Neuron;
using namespace Neuron::Graphics;

void JumpgateUI::Initialize(ClientWorldState* _worldState,
  Camera* _camera)
{
  m_worldState = _worldState;
  m_camera     = _camera;
}

EntityHandle JumpgateUI::FindNearbyJumpgate(EntityHandle _shipHandle) const
{
  if (!m_worldState) return EntityHandle::Invalid();

  const ClientEntity* ship = m_worldState->GetEntity(_shipHandle);
  if (!ship) return EntityHandle::Invalid();

  XMVECTOR shipPos = XMLoadFloat3(&ship->Position);
  float range2 = WarpRange * WarpRange;

  EntityHandle nearest = EntityHandle::Invalid();
  float nearestDist2 = FLT_MAX;

  m_worldState->ForEachActive([&](const ClientEntity& _entity)
  {
    if (_entity.Category != SpaceObjectCategory::Jumpgate) return;

    XMVECTOR gatePos = XMLoadFloat3(&_entity.Position);
    XMVECTOR diff = XMVectorSubtract(gatePos, shipPos);
    float dist2 = XMVectorGetX(XMVector3Dot(diff, diff));

    if (dist2 <= range2 && dist2 < nearestDist2)
    {
      nearestDist2 = dist2;
      nearest = _entity.Handle;
    }
  });

  return nearest;
}

void JumpgateUI::Update(EntityHandle _flagship)
{
  m_nearbyGate = FindNearbyJumpgate(_flagship);
  m_warpAvailable = m_nearbyGate.IsValid();
}

void JumpgateUI::Render(ID3D12GraphicsCommandList* _cmdList,
  ConstantBufferAllocator& _cbAlloc,
  SpriteBatch& _spriteBatch,
  UINT _screenWidth, UINT _screenHeight)
{
  if (!m_warpAvailable) return;

  _spriteBatch.Begin(_cmdList, _cbAlloc, _screenWidth, _screenHeight);

  // Warp indicator in bottom-right corner
  constexpr LONG INDICATOR_SIZE = 40;
  constexpr LONG MARGIN = 20;

  LONG right  = static_cast<LONG>(_screenWidth) - MARGIN;
  LONG bottom = static_cast<LONG>(_screenHeight) - MARGIN;
  LONG left   = right - INDICATOR_SIZE;
  LONG top    = bottom - INDICATOR_SIZE;

  // Background
  XMVECTORF32 bgColor = { 0.0f, 0.0f, 0.2f, 0.6f };
  RECT bgRect = { left, top, right, bottom };
  _spriteBatch.DrawRect(bgRect, bgColor);

  // Pulsing border (warp-ready effect)
  XMVECTORF32 borderColor = { 0.3f, 0.6f, 1.0f, 0.9f };
  RECT bTop = { left, top, right, top + 2 };
  RECT bBot = { left, bottom - 2, right, bottom };
  RECT bLft = { left, top, left + 2, bottom };
  RECT bRgt = { right - 2, top, right, bottom };
  _spriteBatch.DrawRect(bTop, borderColor);
  _spriteBatch.DrawRect(bBot, borderColor);
  _spriteBatch.DrawRect(bLft, borderColor);
  _spriteBatch.DrawRect(bRgt, borderColor);

  // Inner diamond indicator
  LONG cx = left + INDICATOR_SIZE / 2;
  LONG cy = top + INDICATOR_SIZE / 2;
  constexpr LONG DS = 8;
  XMVECTORF32 diamondColor = { 0.4f, 0.8f, 1.0f, 0.9f };
  // Approximate diamond with a small centered square
  RECT diamond = { cx - DS, cy - DS, cx + DS, cy + DS };
  _spriteBatch.DrawRect(diamond, diamondColor);

  _spriteBatch.End();
}
