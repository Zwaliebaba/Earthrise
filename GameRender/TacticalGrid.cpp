#include "pch.h"
#include "TacticalGrid.h"
#include "GpuResourceManager.h"
#include "CompiledShaders/GridVS.h"
#include "CompiledShaders/GridPS.h"

using namespace Neuron::Graphics;

namespace
{
  struct GridVertex
  {
    XMFLOAT3 Position;
  };
}

void TacticalGrid::Initialize()
{
  // Root signature: 1 root CBV
  CD3DX12_ROOT_PARAMETER rootParam;
  rootParam.InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
  rootSigDesc.Init(1, &rootParam, 0, nullptr,
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  m_rootSignature = PipelineHelpers::CreateRootSignature(rootSigDesc);
  SetName(m_rootSignature.get(), L"GridRootSig");

  D3D12_INPUT_ELEMENT_DESC inputLayout[] =
  {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };

  m_pso = PsoBuilder()
    .WithRootSignature(m_rootSignature.get())
    .WithVS(g_pGridVS, sizeof(g_pGridVS))
    .WithPS(g_pGridPS, sizeof(g_pGridPS))
    .WithInputLayout(inputLayout, _countof(inputLayout))
    .AlphaBlend()
    .DepthReadOnly()
    .Topology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE)
    .Build();
  SetName(m_pso.get(), L"GridPSO");

  // Generate grid lines (200m spacing, 2000m extent)
  constexpr float extent = 2000.0f;
  constexpr float spacing = 200.0f;
  constexpr int lineCount = static_cast<int>(extent * 2.0f / spacing) + 1;

  std::vector<GridVertex> vertices;
  vertices.reserve(static_cast<size_t>(lineCount) * 4); // 2 endpoints per line, 2 axes

  for (int i = 0; i < lineCount; ++i)
  {
    float pos = -extent + static_cast<float>(i) * spacing;

    // X-axis line
    vertices.push_back({ { pos, 0, -extent } });
    vertices.push_back({ { pos, 0,  extent } });

    // Z-axis line
    vertices.push_back({ { -extent, 0, pos } });
    vertices.push_back({ {  extent, 0, pos } });
  }

  m_lineVertexCount = static_cast<uint32_t>(vertices.size());
  m_vertexBuffer = GpuResourceManager::CreateStaticBuffer(
    vertices.data(), vertices.size() * sizeof(GridVertex), L"GridVB");
}

void TacticalGrid::Render(ID3D12GraphicsCommandList* cmdList,
  ConstantBufferAllocator& cbAlloc, const Camera& camera, float yLevel)
{
  if (!m_visible) return;

  struct alignas(256) GridConstants
  {
    XMFLOAT4X4 ViewProjection;
    XMFLOAT3 CameraPosition;
    float GridY;
    float GridExtent;
    float FadeStart;
    float FadeEnd;
    float _Pad;
  };

  GridConstants gc{};
  XMStoreFloat4x4(&gc.ViewProjection, XMMatrixTranspose(camera.GetViewProjectionMatrix()));
  XMStoreFloat3(&gc.CameraPosition, camera.GetPosition());
  gc.GridY = yLevel;
  gc.GridExtent = 2000.0f;
  gc.FadeStart = 500.0f;
  gc.FadeEnd = 2000.0f;

  auto alloc = cbAlloc.Allocate<GridConstants>();
  memcpy(alloc.CpuAddress, &gc, sizeof(GridConstants));

  cmdList->SetPipelineState(m_pso.get());
  cmdList->SetGraphicsRootSignature(m_rootSignature.get());
  cmdList->SetGraphicsRootConstantBufferView(0, alloc.GpuAddress);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

  D3D12_VERTEX_BUFFER_VIEW vbView{};
  vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
  vbView.SizeInBytes = m_lineVertexCount * sizeof(GridVertex);
  vbView.StrideInBytes = sizeof(GridVertex);
  cmdList->IASetVertexBuffers(0, 1, &vbView);

  cmdList->DrawInstanced(m_lineVertexCount, 1, 0, 0);
}
