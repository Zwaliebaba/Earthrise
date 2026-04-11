#include "pch.h"
#include "Starfield.h"
#include "GpuResourceManager.h"

using namespace Neuron::Graphics;

namespace
{
  constexpr const char* c_starVS = R"(
cbuffer FrameConstants : register(b0)
{
    float4x4 ViewProjection;
};

struct VSInput
{
    float3 Direction  : POSITION;
    float  Brightness : BRIGHTNESS;
};

struct VSOutput
{
    float4 Position   : SV_Position;
    float  Brightness : BRIGHTNESS;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    // Place star on a large sphere (far but within draw distance)
    float3 worldPos = input.Direction * 19000.0;
    output.Position = mul(float4(worldPos, 1.0), ViewProjection);
    output.Brightness = input.Brightness;
    return output;
}
)";

  constexpr const char* c_starPS = R"(
struct PSInput
{
    float4 Position   : SV_Position;
    float  Brightness : BRIGHTNESS;
};

float4 main(PSInput input) : SV_Target
{
    return float4(input.Brightness, input.Brightness, input.Brightness * 0.95, 1.0);
}
)";

  // Simple deterministic pseudo-random for star placement
  float HashToFloat(uint32_t seed)
  {
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return static_cast<float>(seed) / static_cast<float>(UINT32_MAX);
  }
}

void Starfield::Initialize(uint32_t starCount)
{
  m_starCount = starCount;

  // Generate star positions on a unit sphere with varying brightness
  std::vector<StarVertex> stars(starCount);
  for (uint32_t i = 0; i < starCount; ++i)
  {
    // Fibonacci sphere distribution for even coverage
    float phi = acosf(1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / static_cast<float>(starCount));
    float theta = XM_2PI * static_cast<float>(i) / 1.6180339887f; // Golden ratio

    stars[i].Direction.x = sinf(phi) * cosf(theta);
    stars[i].Direction.y = sinf(phi) * sinf(theta);
    stars[i].Direction.z = cosf(phi);
    stars[i].Brightness = 0.3f + 0.7f * HashToFloat(i * 7919 + 1);
  }

  m_vertexBuffer = GpuResourceManager::CreateStaticBuffer(
    stars.data(), stars.size() * sizeof(StarVertex), L"StarfieldVB");

  // Root signature: 1 root CBV at b0
  CD3DX12_ROOT_PARAMETER rootParam;
  rootParam.InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
  rootSigDesc.Init(1, &rootParam, 0, nullptr,
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  m_rootSignature = PipelineHelpers::CreateRootSignature(rootSigDesc);
  SetName(m_rootSignature.get(), L"StarfieldRootSig");

  // Compile shaders
  auto vsByteCode = PipelineHelpers::CompileShader(
    c_starVS, strlen(c_starVS), "main", "vs_5_1", "StarfieldVS");
  auto psByteCode = PipelineHelpers::CompileShader(
    c_starPS, strlen(c_starPS), "main", "ps_5_1", "StarfieldPS");

  // Input layout
  D3D12_INPUT_ELEMENT_DESC inputLayout[] =
  {
    { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "BRIGHTNESS", 0, DXGI_FORMAT_R32_FLOAT,       0, sizeof(XMFLOAT3),     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };

  // PSO — render as points, reverse-Z depth, no depth write (background)
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
  psoDesc.pRootSignature = m_rootSignature.get();
  psoDesc.VS = { vsByteCode->GetBufferPointer(), vsByteCode->GetBufferSize() };
  psoDesc.PS = { psByteCode->GetBufferPointer(), psByteCode->GetBufferSize() };
  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
  psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Don't write depth for background
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = Core::GetBackBufferFormat();
  psoDesc.DSVFormat = Core::GetDepthBufferFormat();
  psoDesc.SampleDesc.Count = 1;

  m_pso = PipelineHelpers::CreateGraphicsPSO(psoDesc);
  SetName(m_pso.get(), L"StarfieldPSO");
}

void Starfield::Render(ID3D12GraphicsCommandList* cmdList,
  ConstantBufferAllocator& cbAlloc, const Camera& camera)
{
  // Stars always surround the camera (origin-rebased = centered at 0)
  struct StarFrameConstants
  {
    XMFLOAT4X4 ViewProjection;
  };

  StarFrameConstants fc{};
  XMStoreFloat4x4(&fc.ViewProjection, XMMatrixTranspose(camera.GetViewProjectionMatrix()));

  auto alloc = cbAlloc.Allocate<StarFrameConstants>();
  memcpy(alloc.CpuAddress, &fc, sizeof(StarFrameConstants));

  cmdList->SetPipelineState(m_pso.get());
  cmdList->SetGraphicsRootSignature(m_rootSignature.get());
  cmdList->SetGraphicsRootConstantBufferView(0, alloc.GpuAddress);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

  D3D12_VERTEX_BUFFER_VIEW vbView{};
  vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
  vbView.SizeInBytes = m_starCount * sizeof(StarVertex);
  vbView.StrideInBytes = sizeof(StarVertex);
  cmdList->IASetVertexBuffers(0, 1, &vbView);

  cmdList->DrawInstanced(m_starCount, 1, 0, 0);
}
