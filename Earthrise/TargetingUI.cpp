#include "pch.h"
#include "TargetingUI.h"
#include "ClientWorldState.h"
#include "Camera.h"
#include "SpriteBatch.h"

using namespace Neuron;
using namespace Neuron::Graphics;

void TargetingUI::Initialize(ClientWorldState* _worldState,
  Camera* _camera)
{
  m_worldState = _worldState;
  m_camera     = _camera;
}

bool TargetingUI::IsTargetable(SpaceObjectCategory _cat) const noexcept
{
  switch (_cat)
  {
  case SpaceObjectCategory::Ship:     return TargetShips;
  case SpaceObjectCategory::Station:  return TargetStations;
  case SpaceObjectCategory::Asteroid: return TargetAsteroids;
  default: return false;
  }
}

void TargetingUI::RefreshTargetList()
{
  m_targetList.clear();
  if (!m_worldState) return;

  m_worldState->ForEachActive([&](const ClientEntity& _entity)
  {
    if (IsTargetable(_entity.Category))
      m_targetList.push_back(_entity.Handle);
  });

  // Sort by handle ID for stable cycling order
  std::sort(m_targetList.begin(), m_targetList.end(),
    [](const EntityHandle& a, const EntityHandle& b) { return a.m_id < b.m_id; });

  // Revalidate cycle index
  if (m_currentTarget.IsValid())
  {
    auto it = std::find_if(m_targetList.begin(), m_targetList.end(),
      [&](const EntityHandle& h) { return h == m_currentTarget; });
    if (it != m_targetList.end())
      m_cycleIndex = static_cast<int>(std::distance(m_targetList.begin(), it));
    else
    {
      m_currentTarget = EntityHandle::Invalid();
      m_cycleIndex = -1;
    }
  }
}

EntityHandle TargetingUI::CycleNext()
{
  if (m_targetList.empty())
  {
    ClearTarget();
    return EntityHandle::Invalid();
  }

  m_cycleIndex = (m_cycleIndex + 1) % static_cast<int>(m_targetList.size());
  m_currentTarget = m_targetList[m_cycleIndex];
  return m_currentTarget;
}

EntityHandle TargetingUI::CyclePrev()
{
  if (m_targetList.empty())
  {
    ClearTarget();
    return EntityHandle::Invalid();
  }

  m_cycleIndex = (m_cycleIndex - 1 + static_cast<int>(m_targetList.size()))
    % static_cast<int>(m_targetList.size());
  m_currentTarget = m_targetList[m_cycleIndex];
  return m_currentTarget;
}

EntityHandle TargetingUI::SelectNearest()
{
  if (!m_worldState || !m_camera || m_targetList.empty())
  {
    ClearTarget();
    return EntityHandle::Invalid();
  }

  XMVECTOR camPos = m_camera->GetPosition();
  float closestDist = FLT_MAX;
  EntityHandle closest = EntityHandle::Invalid();
  int closestIdx = -1;

  for (int i = 0; i < static_cast<int>(m_targetList.size()); ++i)
  {
    const ClientEntity* ent = m_worldState->GetEntity(m_targetList[i]);
    if (!ent) continue;

    XMVECTOR entPos = XMLoadFloat3(&ent->Position);
    float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(entPos, camPos)));
    if (dist < closestDist)
    {
      closestDist = dist;
      closest = m_targetList[i];
      closestIdx = i;
    }
  }

  m_currentTarget = closest;
  m_cycleIndex = closestIdx;
  return m_currentTarget;
}

void TargetingUI::SetTarget(EntityHandle _handle)
{
  m_currentTarget = _handle;

  auto it = std::find_if(m_targetList.begin(), m_targetList.end(),
    [&](const EntityHandle& h) { return h == _handle; });
  if (it != m_targetList.end())
    m_cycleIndex = static_cast<int>(std::distance(m_targetList.begin(), it));
  else
    m_cycleIndex = -1;
}

void TargetingUI::ClearTarget() noexcept
{
  m_currentTarget = EntityHandle::Invalid();
  m_cycleIndex = -1;
}

bool TargetingUI::ProjectToScreen(const XMFLOAT3& _worldPos,
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

  float ndcX = XMVectorGetX(clipPos) / w;
  float ndcY = XMVectorGetY(clipPos) / w;

  _outX = (ndcX * 0.5f + 0.5f) * static_cast<float>(_screenWidth);
  _outY = (-ndcY * 0.5f + 0.5f) * static_cast<float>(_screenHeight);
  return true;
}

void TargetingUI::Render(ID3D12GraphicsCommandList* _cmdList,
  ConstantBufferAllocator& _cbAlloc,
  SpriteBatch& _spriteBatch,
  UINT _screenWidth, UINT _screenHeight)
{
  if (!m_worldState || !m_camera || !m_currentTarget.IsValid())
    return;

  const ClientEntity* target = m_worldState->GetEntity(m_currentTarget);
  if (!target) return;

  float sx, sy;
  if (!ProjectToScreen(target->Position, _screenWidth, _screenHeight, sx, sy))
    return;

  _spriteBatch.Begin(_cmdList, _cbAlloc, _screenWidth, _screenHeight);

  // Target bracket: red diamond brackets, larger than selection brackets
  constexpr LONG SIZE = 24;
  constexpr LONG THICKNESS = 2;
  XMVECTORF32 bracketColor = { 1.0f, 0.2f, 0.1f, 0.9f };

  LONG cx = static_cast<LONG>(sx);
  LONG cy = static_cast<LONG>(sy);

  // Top-left corner
  RECT r1 = { cx - SIZE, cy - SIZE, cx - SIZE / 2, cy - SIZE + THICKNESS };
  RECT r2 = { cx - SIZE, cy - SIZE, cx - SIZE + THICKNESS, cy - SIZE / 2 };
  _spriteBatch.DrawRect(r1, bracketColor);
  _spriteBatch.DrawRect(r2, bracketColor);

  // Top-right corner
  RECT r3 = { cx + SIZE / 2, cy - SIZE, cx + SIZE, cy - SIZE + THICKNESS };
  RECT r4 = { cx + SIZE - THICKNESS, cy - SIZE, cx + SIZE, cy - SIZE / 2 };
  _spriteBatch.DrawRect(r3, bracketColor);
  _spriteBatch.DrawRect(r4, bracketColor);

  // Bottom-left corner
  RECT r5 = { cx - SIZE, cy + SIZE - THICKNESS, cx - SIZE / 2, cy + SIZE };
  RECT r6 = { cx - SIZE, cy + SIZE / 2, cx - SIZE + THICKNESS, cy + SIZE };
  _spriteBatch.DrawRect(r5, bracketColor);
  _spriteBatch.DrawRect(r6, bracketColor);

  // Bottom-right corner
  RECT r7 = { cx + SIZE / 2, cy + SIZE - THICKNESS, cx + SIZE, cy + SIZE };
  RECT r8 = { cx + SIZE - THICKNESS, cy + SIZE / 2, cx + SIZE, cy + SIZE };
  _spriteBatch.DrawRect(r7, bracketColor);
  _spriteBatch.DrawRect(r8, bracketColor);

  _spriteBatch.End();
}
