#include "pch.h"
#include "FleetSelectionUI.h"
#include "ClientWorldState.h"
#include "InputState.h"
#include "Camera.h"
#include "SpriteBatch.h"

using namespace Neuron;
using namespace Neuron::Graphics;

void FleetSelectionUI::Initialize(ClientWorldState* _worldState,
  Camera* _camera)
{
  m_worldState = _worldState;
  m_camera = _camera;
}

void FleetSelectionUI::Update(float /*_deltaT*/, const InputState& _input)
{
  // Box-drag: start on left-mouse-down, commit on left-mouse-up.
  if (_input.IsLeftMousePressed())
  {
    m_isDragging = true;
    m_dragStartX = _input.GetMouseX();
    m_dragStartY = _input.GetMouseY();
    m_dragEndX = m_dragStartX;
    m_dragEndY = m_dragStartY;
  }

  if (m_isDragging && _input.IsLeftMouseDown())
  {
    m_dragEndX = _input.GetMouseX();
    m_dragEndY = _input.GetMouseY();
  }

  if (m_isDragging && !_input.IsLeftMouseDown())
  {
    m_isDragging = false;
  }
}

void FleetSelectionUI::Render(ID3D12GraphicsCommandList* _cmdList,
  ConstantBufferAllocator& _cbAlloc,
  SpriteBatch& _spriteBatch,
  UINT _screenWidth, UINT _screenHeight)
{
  if (!m_worldState || !m_camera) return;

  _spriteBatch.Begin(_cmdList, _cbAlloc, _screenWidth, _screenHeight);

  // Render selection brackets around selected entities
  for (const auto& handle : m_selectedHandles)
  {
    const ClientEntity* entity = m_worldState->GetEntity(handle);
    if (!entity) continue;

    float sx, sy;
    if (!ProjectToScreen(entity->Position, _screenWidth, _screenHeight, sx, sy))
      continue;

    // Selection bracket: 4 corner L-shapes in cyan
    constexpr LONG BRACKET_SIZE = 15;
    constexpr LONG BRACKET_THICKNESS = 2;
    XMVECTORF32 bracketColor = { 0.0f, 1.0f, 1.0f, 0.8f };

    LONG cx = static_cast<LONG>(sx);
    LONG cy = static_cast<LONG>(sy);

    // Top-left corner
    RECT r1 = { cx - BRACKET_SIZE, cy - BRACKET_SIZE,
                cx - BRACKET_SIZE / 2, cy - BRACKET_SIZE + BRACKET_THICKNESS };
    _spriteBatch.DrawRect(r1, bracketColor);
    RECT r2 = { cx - BRACKET_SIZE, cy - BRACKET_SIZE,
                cx - BRACKET_SIZE + BRACKET_THICKNESS, cy - BRACKET_SIZE / 2 };
    _spriteBatch.DrawRect(r2, bracketColor);

    // Top-right corner
    RECT r3 = { cx + BRACKET_SIZE / 2, cy - BRACKET_SIZE,
                cx + BRACKET_SIZE, cy - BRACKET_SIZE + BRACKET_THICKNESS };
    _spriteBatch.DrawRect(r3, bracketColor);
    RECT r4 = { cx + BRACKET_SIZE - BRACKET_THICKNESS, cy - BRACKET_SIZE,
                cx + BRACKET_SIZE, cy - BRACKET_SIZE / 2 };
    _spriteBatch.DrawRect(r4, bracketColor);

    // Bottom-left corner
    RECT r5 = { cx - BRACKET_SIZE, cy + BRACKET_SIZE - BRACKET_THICKNESS,
                cx - BRACKET_SIZE / 2, cy + BRACKET_SIZE };
    _spriteBatch.DrawRect(r5, bracketColor);
    RECT r6 = { cx - BRACKET_SIZE, cy + BRACKET_SIZE / 2,
                cx - BRACKET_SIZE + BRACKET_THICKNESS, cy + BRACKET_SIZE };
    _spriteBatch.DrawRect(r6, bracketColor);

    // Bottom-right corner
    RECT r7 = { cx + BRACKET_SIZE / 2, cy + BRACKET_SIZE - BRACKET_THICKNESS,
                cx + BRACKET_SIZE, cy + BRACKET_SIZE };
    _spriteBatch.DrawRect(r7, bracketColor);
    RECT r8 = { cx + BRACKET_SIZE - BRACKET_THICKNESS, cy + BRACKET_SIZE / 2,
                cx + BRACKET_SIZE, cy + BRACKET_SIZE };
    _spriteBatch.DrawRect(r8, bracketColor);
  }

  // Render drag box
  if (m_isDragging)
  {
    float ddx = m_dragEndX - m_dragStartX;
    float ddy = m_dragEndY - m_dragStartY;

    if (std::sqrt(ddx * ddx + ddy * ddy) >= DRAG_THRESHOLD)
    {
      LONG x0 = static_cast<LONG>((std::min)(m_dragStartX, m_dragEndX));
      LONG y0 = static_cast<LONG>((std::min)(m_dragStartY, m_dragEndY));
      LONG x1 = static_cast<LONG>((std::max)(m_dragStartX, m_dragEndX));
      LONG y1 = static_cast<LONG>((std::max)(m_dragStartY, m_dragEndY));

      // Fill (semi-transparent green)
      XMVECTORF32 boxColor = { 0.0f, 1.0f, 0.0f, 0.15f };
      RECT fillRect = { x0, y0, x1, y1 };
      _spriteBatch.DrawRect(fillRect, boxColor);

      // Border (brighter green)
      XMVECTORF32 borderColor = { 0.0f, 1.0f, 0.0f, 0.8f };
      RECT top = { x0, y0, x1, y0 + 1 };
      RECT bot = { x0, y1 - 1, x1, y1 };
      RECT lft = { x0, y0, x0 + 1, y1 };
      RECT rgt = { x1 - 1, y0, x1, y1 };
      _spriteBatch.DrawRect(top, borderColor);
      _spriteBatch.DrawRect(bot, borderColor);
      _spriteBatch.DrawRect(lft, borderColor);
      _spriteBatch.DrawRect(rgt, borderColor);
    }
  }

  _spriteBatch.End();
}

EntityHandle FleetSelectionUI::PickEntity(float _screenX, float _screenY) const
{
  if (!m_worldState || !m_camera) return EntityHandle::Invalid();

  // Approximate screen dimensions from camera aspect
  UINT screenW = static_cast<UINT>(m_camera->GetAspect() * 1080.0f);
  UINT screenH = 1080u;

  EntityHandle closest = EntityHandle::Invalid();
  float closestDist = FLT_MAX;

  m_worldState->ForEachActive([&](const ClientEntity& _entity)
  {
    float sx, sy;
    if (!ProjectToScreen(_entity.Position, screenW, screenH, sx, sy))
      return;

    float ddx = sx - _screenX;
    float ddy = sy - _screenY;
    float screenDist = std::sqrt(ddx * ddx + ddy * ddy);

    if (screenDist < SELECT_RADIUS_SCREEN && screenDist < closestDist)
    {
      closestDist = screenDist;
      closest = _entity.Handle;
    }
  });

  return closest;
}

void FleetSelectionUI::BoxSelect(float _x0, float _y0, float _x1, float _y1,
  std::vector<EntityHandle>& _outHandles) const
{
  if (!m_worldState || !m_camera) return;

  UINT screenW = static_cast<UINT>(m_camera->GetAspect() * 1080.0f);
  UINT screenH = 1080u;

  float minX = (std::min)(_x0, _x1);
  float minY = (std::min)(_y0, _y1);
  float maxX = (std::max)(_x0, _x1);
  float maxY = (std::max)(_y0, _y1);

  m_worldState->ForEachActive([&](const ClientEntity& _entity)
  {
    // Only select ships
    if (_entity.Category != SpaceObjectCategory::Ship)
      return;

    float sx, sy;
    if (!ProjectToScreen(_entity.Position, screenW, screenH, sx, sy))
      return;

    if (sx >= minX && sx <= maxX && sy >= minY && sy <= maxY)
      _outHandles.push_back(_entity.Handle);
  });
}

void FleetSelectionUI::SetSelection(const std::vector<EntityHandle>& _selection)
{
  m_selectedHandles = _selection;
}

bool FleetSelectionUI::ProjectToScreen(const XMFLOAT3& _worldPos,
  UINT _screenWidth, UINT _screenHeight,
  float& _outX, float& _outY) const
{
  XMVECTOR worldPos = XMLoadFloat3(&_worldPos);
  XMVECTOR rebasedPos = m_camera->RebasePosition(worldPos);

  XMMATRIX viewProj = m_camera->GetViewProjectionMatrix();
  XMVECTOR clipPos = XMVector4Transform(
    XMVectorSet(XMVectorGetX(rebasedPos), XMVectorGetY(rebasedPos),
      XMVectorGetZ(rebasedPos), 1.0f), viewProj);

  float w = XMVectorGetW(clipPos);
  if (w <= 0.0f)
    return false;

  // Perspective divide
  float ndcX = XMVectorGetX(clipPos) / w;
  float ndcY = XMVectorGetY(clipPos) / w;

  // NDC to screen space (origin top-left)
  _outX = (ndcX * 0.5f + 0.5f) * static_cast<float>(_screenWidth);
  _outY = (-ndcY * 0.5f + 0.5f) * static_cast<float>(_screenHeight);

  return true;
}
