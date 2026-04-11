#include "pch.h"
#include "TacticalGrid.h"
#include "GpuResourceManager.h"

using namespace Neuron::Graphics;

namespace
{
  constexpr const char* c_gridVS = R"(
cbuffer GridConstants : register(b0)
{
    float4x4 ViewProjection;
    float3   CameraPosition;
    float    GridY;
    float    GridExtent;
    float    FadeStart;
    float    FadeEnd;
    float    _Pad;
};

struct VSInput
{
    float3 Position : POSITION;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float  Alpha    : ALPHA;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float3 worldPos = float3(input.Position.x, GridY, input.Position.z);
    output.Position = mul(float4(worldPos, 1.0), ViewProjection);

    // Distance fade
    float dist = length(worldPos - CameraPosition);
    output.Alpha = 1.0 - saturate((dist - FadeStart) / (FadeEnd - FadeStart));

    return output;
}
)";

  constexpr const char* c_gridPS = R"(
struct PSInput
{
    float4 Position : SV_Position;
    float  Alpha    : ALPHA;
};

float4 main(PSInput input) : SV_Target
{
    // Cyan/blue grid lines
    return float4(0.1, 0.6, 0.8, input.Alpha * 0.5);
}
)";

  struct GridVertex
  {
    XMFLOAT3 Position;
  };
}

void TacticalGrid::Initialize()
{
  auto vsCode = PipelineHelpers::CompileShader(c_gridVS, strlen(c_gridVS), "main", "vs_5_1", "GridVS");
  auto psCode = PipelineHelpers::CompileShader(c_gridPS, strlen(c_gridPS), "main", "ps_5_1", "GridPS");

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

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
  psoDesc.pRootSignature = m_rootSignature.get();
  psoDesc.VS = { vsCode->GetBufferPointer(), vsCode->GetBufferSize() };
  psoDesc.PS = { psCode->GetBufferPointer(), psCode->GetBufferSize() };
  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

  // Alpha blending for distance fade
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
  psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
  psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
  psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
  psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Transparent, no depth write
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = Core::GetBackBufferFormat();
  psoDesc.DSVFormat = Core::GetDepthBufferFormat();
  psoDesc.SampleDesc.Count = 1;

  m_pso = PipelineHelpers::CreateGraphicsPSO(psoDesc);
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
