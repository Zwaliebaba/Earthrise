#include "pch.h"
#include "VertexColorPipeline.h"

using namespace Neuron::Graphics;

namespace
{
  // Landscape vertex shader — single-pass with barycentric wireframe.
  // Transforms position, computes per-vertex lighting, passes barycentric
  // coordinates (encoded in TexCoord0) for edge detection in the pixel shader.
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
};

struct VSInput
{
    float3 Position  : POSITION;
    float3 Normal    : NORMAL;
    float4 Color     : COLOR;
    float2 TexCoord0 : TEXCOORD0;
};

struct VSOutput
{
    float4 Position  : SV_POSITION;
    float4 Color     : COLOR0;
    float2 Bary      : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    // Transform position
    float4 worldPos = mul(float4(input.Position, 1.0f), World);
    output.Position = mul(worldPos, ViewProjection);

    // Pass barycentric coordinates for wireframe edge detection
    output.Bary = input.TexCoord0;

    // ---- Lighting (always on for landscape) ----
    float3 normal = normalize(mul(input.Normal, (float3x3) World));

    // Color-material: vertex colour drives ambient + diffuse
    float4 matAmbient = input.Color;
    float4 matDiffuse = input.Color;

    float3 ambient = AmbientIntensity * matAmbient.rgb;

    // Simple directional light
    float3 lightDir = normalize(-LightDirection);
    float NdotL = max(dot(normal, lightDir), 0.0f);
    float3 diffuse = NdotL * matDiffuse.rgb;

    float3 finalColor = ambient + diffuse;
    output.Color = float4(saturate(finalColor), matDiffuse.a);

    return output;
}
)";

  // Landscape pixel shader — barycentric wireframe.
  //
  // Uses interpolated barycentric coordinates to detect triangle edges.
  // Pixels near any edge (where a barycentric component approaches 0)
  // receive an additive highlight tinted by the surface colour,
  // reproducing the Darwinia triangle-outline aesthetic.
  constexpr const char* c_psSource = R"(
struct PSInput
{
    float4 Position  : SV_POSITION;
    float4 Color     : COLOR0;
    float2 Bary      : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    // Base terrain colour (vertex-lit)
    float3 baseColor = input.Color.rgb;

    // Reconstruct 3-component barycentrics from the 2 interpolated values
    float3 bary = float3(input.Bary, 1.0 - input.Bary.x - input.Bary.y);

    // Screen-space anti-aliased wireframe: use fwidth for consistent line width
    float3 fw = fwidth(bary);
    float3 edgeFactor = smoothstep(float3(0,0,0), fw * 1.2, bary);
    float edge = 1.0 - min(edgeFactor.x, min(edgeFactor.y, edgeFactor.z));

    // Additive edge highlight tinted by surface colour (Darwinia style)
    float3 color = baseColor + edge * baseColor;

    return float4(color, 1.0f);
}
)";
}

void VertexColorPipeline::Initialize()
{
  auto vsByteCode = PipelineHelpers::CompileShader(
    c_vsSource, strlen(c_vsSource), "main", "vs_5_1", "LandscapeVS");
  auto psByteCode = PipelineHelpers::CompileShader(
    c_psSource, strlen(c_psSource), "main", "ps_5_1", "LandscapePS");

  // Root signature: 2 root CBVs (b0, b1), no texture bindings
  CD3DX12_ROOT_PARAMETER rootParams[2];
  rootParams[ROOT_PARAM_FRAME_CBV].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
  rootParams[ROOT_PARAM_DRAW_CBV].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL);

  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
  rootSigDesc.Init(_countof(rootParams), rootParams, 0, nullptr,
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  m_rootSignature = PipelineHelpers::CreateRootSignature(rootSigDesc);
  SetName(m_rootSignature.get(), L"LandscapeRootSig");

  // Input layout: Position + Normal + Color + TexCoord0
  D3D12_INPUT_ELEMENT_DESC inputLayout[] =
  {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,                            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, sizeof(XMFLOAT3),             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, sizeof(XMFLOAT3) * 2,        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,        0, sizeof(XMFLOAT3) * 2 + sizeof(XMFLOAT4), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };

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
  SetName(m_pso.get(), L"LandscapePSO");
}

void VertexColorPipeline::BindPipeline(ID3D12GraphicsCommandList* cmdList) const
{
  cmdList->SetPipelineState(m_pso.get());
  cmdList->SetGraphicsRootSignature(m_rootSignature.get());
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void VertexColorPipeline::SetFrameConstants(ID3D12GraphicsCommandList* cmdList,
  ConstantBufferAllocator& cbAlloc, const FrameConstants& data) const
{
  auto alloc = cbAlloc.Allocate<FrameConstants>();
  memcpy(alloc.CpuAddress, &data, sizeof(FrameConstants));
  cmdList->SetGraphicsRootConstantBufferView(ROOT_PARAM_FRAME_CBV, alloc.GpuAddress);
}

void VertexColorPipeline::SetDrawConstants(ID3D12GraphicsCommandList* cmdList,
  ConstantBufferAllocator& cbAlloc, const SurfaceDrawConstants& data) const
{
  auto alloc = cbAlloc.Allocate<SurfaceDrawConstants>();
  memcpy(alloc.CpuAddress, &data, sizeof(SurfaceDrawConstants));
  cmdList->SetGraphicsRootConstantBufferView(ROOT_PARAM_DRAW_CBV, alloc.GpuAddress);
}
