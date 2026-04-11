#include "pch.h"
#include "SpriteBatch.h"

using namespace Neuron::Graphics;

namespace
{
  constexpr const char* c_spriteVS = R"(
cbuffer SpriteConstants : register(b0)
{
    float4x4 Projection;
};

struct VSInput
{
    float2 Position : POSITION;
    float4 Color    : COLOR;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float4 Color    : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.Position = mul(float4(input.Position, 0.0, 1.0), Projection);
    output.Color = input.Color;
    return output;
}
)";

  constexpr const char* c_spritePS = R"(
struct PSInput
{
    float4 Position : SV_Position;
    float4 Color    : COLOR;
};

float4 main(PSInput input) : SV_Target
{
    return input.Color;
}
)";
}

void SpriteBatch::Initialize()
{
  auto vsCode = PipelineHelpers::CompileShader(c_spriteVS, strlen(c_spriteVS), "main", "vs_5_1", "SpriteVS");
  auto psCode = PipelineHelpers::CompileShader(c_spritePS, strlen(c_spritePS), "main", "ps_5_1", "SpritePS");

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

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
  psoDesc.pRootSignature = m_rootSignature.get();
  psoDesc.VS = { vsCode->GetBufferPointer(), vsCode->GetBufferSize() };
  psoDesc.PS = { psCode->GetBufferPointer(), psCode->GetBufferSize() };
  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

  // Alpha blending for UI
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
  psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
  psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
  psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

  psoDesc.DepthStencilState.DepthEnable = FALSE;
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = Core::GetBackBufferFormat();
  psoDesc.SampleDesc.Count = 1;

  m_pso = PipelineHelpers::CreateGraphicsPSO(psoDesc);
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
