#include "pch.h"
#include "FlatColorPipeline.h"

using namespace Neuron::Graphics;

namespace
{
  // Embedded HLSL — compiled at initialization time via D3DCompile
  constexpr const char* c_vsSource = R"(
cbuffer FrameConstants : register(b0)
{
    float4x4 ViewProjection;
    float3   CameraPosition;
    float    _Pad0;
    float3   LightDirection;
    float    AmbientIntensity;
};

cbuffer DrawConstants : register(b1)
{
    float4x4 World;
    float4   Color;
};

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
};

struct VSOutput
{
    float4 Position  : SV_Position;
    float3 WorldNormal   : NORMAL;
    float3 WorldPosition : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(float4(input.Position, 1.0), World);
    output.Position      = mul(worldPos, ViewProjection);
    output.WorldNormal   = normalize(mul(input.Normal, (float3x3) World));
    output.WorldPosition = worldPos.xyz;
    return output;
}
)";

  constexpr const char* c_psSource = R"(
cbuffer FrameConstants : register(b0)
{
    float4x4 ViewProjection;
    float3   CameraPosition;
    float    _Pad0;
    float3   LightDirection;
    float    AmbientIntensity;
};

cbuffer DrawConstants : register(b1)
{
    float4x4 World;
    float4   Color;
};

struct PSInput
{
    float4 Position      : SV_Position;
    float3 WorldNormal   : NORMAL;
    float3 WorldPosition : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    float3 N = normalize(input.WorldNormal);
    float3 L = normalize(-LightDirection);
    float3 V = normalize(CameraPosition - input.WorldPosition);
    float3 H = normalize(L + V);

    // Hemisphere ambient: brighter from above (sky), darker below (ground)
    float skyBlend = N.y * 0.5 + 0.5;
    float ambient  = lerp(0.25, 0.65, skyBlend) * AmbientIntensity;

    // Half-Lambert diffuse: softer shadow falloff than standard Lambert
    float NdotL      = dot(N, L);
    float halfLambert = NdotL * 0.5 + 0.5;
    float diffuse     = halfLambert * halfLambert;

    // Blinn-Phong specular highlight
    float NdotH = saturate(dot(N, H));
    float spec  = pow(NdotH, 40.0) * 0.25 * step(0.0, NdotL);

    // Fresnel rim for silhouette readability in space
    float NdotV = saturate(dot(N, V));
    float rim   = pow(1.0 - NdotV, 3.0) * 0.12;

    float3 result = Color.rgb * (ambient + diffuse * (1.0 - AmbientIntensity))
                  + spec
                  + rim * Color.rgb;

    return float4(result, Color.a);
}
)";
}

void FlatColorPipeline::Initialize()
{
  // Compile shaders
  auto vsByteCode = PipelineHelpers::CompileShader(
    c_vsSource, strlen(c_vsSource), "main", "vs_5_1", "FlatColorVS");
  auto psByteCode = PipelineHelpers::CompileShader(
    c_psSource, strlen(c_psSource), "main", "ps_5_1", "FlatColorPS");

  // Root signature: 2 root CBVs (b0 = frame, b1 = draw)
  CD3DX12_ROOT_PARAMETER rootParams[2];
  rootParams[ROOT_PARAM_FRAME_CBV].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
  rootParams[ROOT_PARAM_DRAW_CBV].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL);

  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
  rootSigDesc.Init(_countof(rootParams), rootParams, 0, nullptr,
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  m_rootSignature = PipelineHelpers::CreateRootSignature(rootSigDesc);
  SetName(m_rootSignature.get(), L"FlatColorRootSig");

  // Input layout: Position + Normal
  D3D12_INPUT_ELEMENT_DESC inputLayout[] =
  {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(XMFLOAT3),     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };

  // PSO
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
  psoDesc.pRootSignature = m_rootSignature.get();
  psoDesc.VS = { vsByteCode->GetBufferPointer(), vsByteCode->GetBufferSize() };
  psoDesc.PS = { psByteCode->GetBufferPointer(), psByteCode->GetBufferSize() };
  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  psoDesc.RasterizerState.FrontCounterClockwise = TRUE; // CMO meshes use CCW winding
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

  // Reverse-Z depth: GREATER_EQUAL comparison, depth write enabled
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = Core::GetBackBufferFormat();
  psoDesc.DSVFormat = Core::GetDepthBufferFormat();
  psoDesc.SampleDesc.Count = 1;

  m_pso = PipelineHelpers::CreateGraphicsPSO(psoDesc);
  SetName(m_pso.get(), L"FlatColorPSO");
}

void FlatColorPipeline::BindPipeline(ID3D12GraphicsCommandList* cmdList) const
{
  cmdList->SetPipelineState(m_pso.get());
  cmdList->SetGraphicsRootSignature(m_rootSignature.get());
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void FlatColorPipeline::SetFrameConstants(ID3D12GraphicsCommandList* cmdList,
  ConstantBufferAllocator& cbAlloc, const FrameConstants& data) const
{
  auto alloc = cbAlloc.Allocate<FrameConstants>();
  memcpy(alloc.CpuAddress, &data, sizeof(FrameConstants));
  cmdList->SetGraphicsRootConstantBufferView(ROOT_PARAM_FRAME_CBV, alloc.GpuAddress);
}

void FlatColorPipeline::SetDrawConstants(ID3D12GraphicsCommandList* cmdList,
  ConstantBufferAllocator& cbAlloc, const DrawConstants& data) const
{
  auto alloc = cbAlloc.Allocate<DrawConstants>();
  memcpy(alloc.CpuAddress, &data, sizeof(DrawConstants));
  cmdList->SetGraphicsRootConstantBufferView(ROOT_PARAM_DRAW_CBV, alloc.GpuAddress);
}
