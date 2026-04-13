#include "pch.h"
#include "SpriteBatch.h"
#include "CompiledShaders/SpriteVS.h"
#include "CompiledShaders/SpritePS.h"

using namespace Neuron::Graphics;

void SpriteBatch::Initialize()
{
  CD3DX12_ROOT_PARAMETER rootParam;
  rootParam.InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
  rootSigDesc.Init(1, &rootParam, 0, nullptr,
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  m_rootSignature = PipelineHelpers::CreateRootSignature(rootSigDesc);
  SetName(m_rootSignature.get(), L"SpriteBatchRootSig");

  D3D12_INPUT_ELEMENT_DESC inputLayout[] =
  {
    { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0,                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, sizeof(XMFLOAT2),    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };

  m_pso = PsoBuilder()
    .WithRootSignature(m_rootSignature.get())
    .WithVS(g_pSpriteVS, sizeof(g_pSpriteVS))
    .WithPS(g_pSpritePS, sizeof(g_pSpritePS))
    .WithInputLayout(inputLayout, _countof(inputLayout))
    .NoCull()
    .AlphaBlend()
    .NoDepth()
    .Build();
  SetName(m_pso.get(), L"SpriteBatchPSO");

  m_vertices.reserve(MAX_SPRITES_PER_BATCH * VERTICES_PER_SPRITE);
}

void SpriteBatch::Begin(ID3D12GraphicsCommandList* cmdList,
  ConstantBufferAllocator& cbAlloc,
  UINT screenWidth, UINT screenHeight)
{
  m_cmdList = cmdList;
  m_cbAlloc = &cbAlloc;
  m_vertices.clear();

  // Orthographic projection (screen space: origin top-left)
  XMMATRIX proj = XMMatrixOrthographicOffCenterLH(
    0, static_cast<float>(screenWidth),
    static_cast<float>(screenHeight), 0,
    0, 1);

  struct { XMFLOAT4X4 Projection; } data;
  XMStoreFloat4x4(&data.Projection, XMMatrixTranspose(proj));

  auto alloc = cbAlloc.Allocate(sizeof(data));
  memcpy(alloc.CpuAddress, &data, sizeof(data));

  cmdList->SetPipelineState(m_pso.get());
  cmdList->SetGraphicsRootSignature(m_rootSignature.get());
  cmdList->SetGraphicsRootConstantBufferView(0, alloc.GpuAddress);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void XM_CALLCONV SpriteBatch::DrawRect(const RECT& dest, FXMVECTOR color)
{
  XMFLOAT4 col;
  XMStoreFloat4(&col, color);

  const float left = static_cast<float>(dest.left);
  const float top = static_cast<float>(dest.top);
  const float right = static_cast<float>(dest.right);
  const float bottom = static_cast<float>(dest.bottom);

  // Two triangles (6 vertices)
  m_vertices.push_back({ { left, top }, col });
  m_vertices.push_back({ { right, top }, col });
  m_vertices.push_back({ { left, bottom }, col });
  m_vertices.push_back({ { right, top }, col });
  m_vertices.push_back({ { right, bottom }, col });
  m_vertices.push_back({ { left, bottom }, col });

  if (m_vertices.size() >= MAX_SPRITES_PER_BATCH * VERTICES_PER_SPRITE)
    FlushBatch();
}

void SpriteBatch::End()
{
  if (!m_vertices.empty())
    FlushBatch();
  m_cmdList = nullptr;
  m_cbAlloc = nullptr;
}

void SpriteBatch::FlushBatch()
{
  if (m_vertices.empty() || !m_cmdList) return;

  const size_t dataSize = m_vertices.size() * sizeof(SpriteVertex);
  auto alloc = m_cbAlloc->Allocate(dataSize);
  memcpy(alloc.CpuAddress, m_vertices.data(), dataSize);

  D3D12_VERTEX_BUFFER_VIEW vbView{};
  vbView.BufferLocation = alloc.GpuAddress;
  vbView.SizeInBytes = static_cast<UINT>(dataSize);
  vbView.StrideInBytes = sizeof(SpriteVertex);
  m_cmdList->IASetVertexBuffers(0, 1, &vbView);

  m_cmdList->DrawInstanced(static_cast<UINT>(m_vertices.size()), 1, 0, 0);
  m_vertices.clear();
}
