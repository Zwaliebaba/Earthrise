#include "pch.h"
#include "SunBillboard.h"
#include "GpuResourceManager.h"
#include "DdsLoader.h"

using namespace Neuron::Graphics;

namespace
{
  constexpr const char* c_billboardVS = R"(
cbuffer BillboardCB : register(b0)
{
    float4x4 ViewProjection;
    float3   CameraRight;
    float    Radius;
    float3   CameraUp;
    float    _Pad0;
    float3   Center;
    float    _Pad1;
    float4   Color;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

static const float2 QuadUVs[6] = {
    {0,0}, {1,0}, {0,1},
    {1,0}, {1,1}, {0,1}
};
static const float2 QuadOffsets[6] = {
    {-1,+1}, {+1,+1}, {-1,-1},
    {+1,+1}, {+1,-1}, {-1,-1}
};

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;
    float2 offset = QuadOffsets[vertexId] * Radius;
    output.TexCoord = QuadUVs[vertexId];

    float3 worldPos = Center
        + CameraRight * offset.x
        + CameraUp    * offset.y;

    output.Position = mul(float4(worldPos, 1.0), ViewProjection);
    return output;
}
)";

  constexpr const char* c_billboardPS = R"(
Texture2D GlowTexture : register(t0);
SamplerState LinearSampler : register(s0);

cbuffer BillboardCB : register(b0)
{
    float4x4 ViewProjection;
    float3   CameraRight;
    float    Radius;
    float3   CameraUp;
    float    _Pad0;
    float3   Center;
    float    _Pad1;
    float4   Color;
};

float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float4 texel = GlowTexture.Sample(LinearSampler, uv);
    return float4(texel.rgb * Color.rgb, texel.a * Color.a);
}
)";
}

void SunBillboard::Initialize()
{
  // Load Glow.dds texture
  std::wstring texPath = FileSys::GetHomeDirectory();
  texPath += L"Textures\\Glow.dds";
  CpuImage image = DdsLoader::LoadFromFile(texPath);
  if (image.Pixels.empty())
  {
    Neuron::DebugTrace("SunBillboard: Glow.dds not found\n");
    return;
  }

  m_glowTexture = GpuResourceManager::CreateStaticTexture(image, L"GlowTexture");

  // Non-shader-visible SRV for texture (we'll copy to shader-visible at render time)
  m_glowSRV_CPU = Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = m_glowTexture->GetDesc().Format;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Texture2D.MipLevels = 1;
  Core::GetD3DDevice()->CreateShaderResourceView(m_glowTexture.get(), &srvDesc, m_glowSRV_CPU);

  // Root signature: b0 = CBV, t0 = SRV descriptor table, s0 = static sampler
  CD3DX12_ROOT_PARAMETER rootParams[2];
  rootParams[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

  CD3DX12_DESCRIPTOR_RANGE srvRange;
  srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  rootParams[1].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

  D3D12_STATIC_SAMPLER_DESC sampler{};
  sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.ShaderRegister = 0;
  sampler.RegisterSpace = 0;
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  sampler.MaxLOD = D3D12_FLOAT32_MAX;

  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
  rootSigDesc.Init(_countof(rootParams), rootParams, 1, &sampler,
    D3D12_ROOT_SIGNATURE_FLAG_NONE);

  m_rootSignature = PipelineHelpers::CreateRootSignature(rootSigDesc);
  SetName(m_rootSignature.get(), L"SunBillboardRootSig");

  // Compile shaders
  auto vsByteCode = PipelineHelpers::CompileShader(
    c_billboardVS, strlen(c_billboardVS), "main", "vs_5_1", "SunBillboardVS");
  auto psByteCode = PipelineHelpers::CompileShader(
    c_billboardPS, strlen(c_billboardPS), "main", "ps_5_1", "SunBillboardPS");

  // PSO — additive blend, no depth write, depth test enabled
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.pRootSignature = m_rootSignature.get();
  psoDesc.VS = { vsByteCode->GetBufferPointer(), vsByteCode->GetBufferSize() };
  psoDesc.PS = { psByteCode->GetBufferPointer(), psByteCode->GetBufferSize() };
  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

  // Alpha-modulated additive blending: src.a * src + dst
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
  psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
  psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
  psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
  psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  // Depth: test enabled (reverse-Z), write disabled (transparent effect)
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
  psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = Core::GetBackBufferFormat();
  psoDesc.DSVFormat = Core::GetDepthBufferFormat();
  psoDesc.SampleDesc.Count = 1;

  m_pso = PipelineHelpers::CreateGraphicsPSO(psoDesc);
  SetName(m_pso.get(), L"SunBillboardPSO");
}

void XM_CALLCONV SunBillboard::Render(
  ID3D12GraphicsCommandList* _cmdList,
  ConstantBufferAllocator& _cbAlloc,
  ShaderVisibleHeap& _srvHeap,
  const Camera& _camera,
  FXMVECTOR _rebasedPosition,
  float _visualRadius,
  FXMVECTOR _color)
{
  if (!m_pso) return; // Not initialized (texture missing)

  BillboardConstants cb{};
  XMStoreFloat4x4(&cb.ViewProjection, XMMatrixTranspose(_camera.GetViewProjectionMatrix()));
  XMStoreFloat3(&cb.CameraRight, _camera.GetRight());
  cb.Radius = _visualRadius;
  XMStoreFloat3(&cb.CameraUp, _camera.GetUp());
  XMStoreFloat3(&cb.Center, _rebasedPosition);
  XMStoreFloat4(&cb.Color, _color);

  auto alloc = _cbAlloc.Allocate<BillboardConstants>();
  memcpy(alloc.CpuAddress, &cb, sizeof(BillboardConstants));

  // Copy SRV to shader-visible heap
  auto srvHandle = _srvHeap.Allocate(1);
  Core::GetD3DDevice()->CopyDescriptorsSimple(1,
    static_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(srvHandle),
    m_glowSRV_CPU,
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  // Bind the shader-visible descriptor heap before setting descriptor tables
  ID3D12DescriptorHeap* heaps[] = { _srvHeap.GetHeap() };
  _cmdList->SetDescriptorHeaps(1, heaps);

  _cmdList->SetPipelineState(m_pso.get());
  _cmdList->SetGraphicsRootSignature(m_rootSignature.get());
  _cmdList->SetGraphicsRootConstantBufferView(0, alloc.GpuAddress);
  _cmdList->SetGraphicsRootDescriptorTable(1, srvHandle);
  _cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  _cmdList->DrawInstanced(6, 1, 0, 0);
}
