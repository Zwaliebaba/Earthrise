#include "pch.h"
#include "PostProcess.h"

using namespace Neuron::Graphics;

namespace
{
  constexpr const char* c_fullscreenVS = R"(
struct VSOutput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;
    // Full-screen triangle: 3 vertices, no vertex buffer needed
    output.TexCoord = float2((vertexId << 1) & 2, vertexId & 2);
    output.Position = float4(output.TexCoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}
)";

  constexpr const char* c_bloomExtractPS = R"(
cbuffer BloomConstants : register(b0)
{
    float Threshold;
    float3 _Pad;
};

Texture2D SceneTexture : register(t0);
SamplerState LinearSampler : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

float4 main(PSInput input) : SV_Target
{
    float4 color = SceneTexture.Sample(LinearSampler, input.TexCoord);
    float brightness = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
    float contribution = max(0, brightness - Threshold);
    return float4(color.rgb * (contribution / max(brightness, 0.001)), 1.0);
}
)";

  constexpr const char* c_bloomBlurPS = R"(
cbuffer BlurConstants : register(b0)
{
    float2 TexelSize;
    float2 _Pad;
};

Texture2D SourceTexture : register(t0);
SamplerState LinearSampler : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

static const float weights[5] = { 0.227027, 0.194946, 0.121622, 0.054054, 0.016216 };

float4 main(PSInput input) : SV_Target
{
    float3 result = SourceTexture.Sample(LinearSampler, input.TexCoord).rgb * weights[0];

    for (int i = 1; i < 5; ++i)
    {
        float2 offset = float2(TexelSize.x * i, TexelSize.y * i);
        result += SourceTexture.Sample(LinearSampler, input.TexCoord + offset).rgb * weights[i];
        result += SourceTexture.Sample(LinearSampler, input.TexCoord - offset).rgb * weights[i];
    }

    return float4(result, 1.0);
}
)";

  constexpr const char* c_bloomCompositePS = R"(
Texture2D SceneTexture : register(t0);
Texture2D BloomTexture : register(t1);
SamplerState LinearSampler : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

float4 main(PSInput input) : SV_Target
{
    float4 scene = SceneTexture.Sample(LinearSampler, input.TexCoord);
    float4 bloom = BloomTexture.Sample(LinearSampler, input.TexCoord);
    return float4(scene.rgb + bloom.rgb, scene.a);
}
)";
}

void PostProcess::Initialize(UINT width, UINT height)
{
  // Compile shaders
  auto vsCode = PipelineHelpers::CompileShader(c_fullscreenVS, strlen(c_fullscreenVS), "main", "vs_5_1", "FullscreenVS");
  auto extractPS = PipelineHelpers::CompileShader(c_bloomExtractPS, strlen(c_bloomExtractPS), "main", "ps_5_1", "BloomExtractPS");
  auto blurPS = PipelineHelpers::CompileShader(c_bloomBlurPS, strlen(c_bloomBlurPS), "main", "ps_5_1", "BloomBlurPS");
  auto compositePS = PipelineHelpers::CompileShader(c_bloomCompositePS, strlen(c_bloomCompositePS), "main", "ps_5_1", "BloomCompositePS");

  // Root signature: 1 CBV (b0) + 1 descriptor table (SRVs t0-t1) + 1 static sampler
  CD3DX12_DESCRIPTOR_RANGE srvRange;
  srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0); // t0, t1

  CD3DX12_ROOT_PARAMETER rootParams[2];
  rootParams[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
  rootParams[1].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

  CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
  rootSigDesc.Init(_countof(rootParams), rootParams, 1, &sampler,
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  m_rootSignature = PipelineHelpers::CreateRootSignature(rootSigDesc);
  SetName(m_rootSignature.get(), L"PostProcessRootSig");

  // Common PSO state (no depth, full-screen triangle, no input layout)
  auto makePSO = [&](ID3DBlob* ps) -> com_ptr<ID3D12PipelineState>
  {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_rootSignature.get();
    desc.VS = { vsCode->GetBufferPointer(), vsCode->GetBufferSize() };
    desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.SampleMask = UINT_MAX;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = Core::GetBackBufferFormat();
    desc.SampleDesc.Count = 1;
    return PipelineHelpers::CreateGraphicsPSO(desc);
  };

  m_extractPSO = makePSO(extractPS.get());
  SetName(m_extractPSO.get(), L"BloomExtractPSO");

  m_blurPSO = makePSO(blurPS.get());
  SetName(m_blurPSO.get(), L"BloomBlurPSO");

  m_compositePSO = makePSO(compositePS.get());
  SetName(m_compositePSO.get(), L"BloomCompositePSO");

  CreateResources(width, height);
}

void PostProcess::OnResize(UINT width, UINT height)
{
  if (width == m_width && height == m_height) return;
  CreateResources(width, height);
}

void PostProcess::CreateResources(UINT width, UINT height)
{
  m_width = width;
  m_height = height;

  const UINT bloomW = width / BLOOM_DOWNSCALE;
  const UINT bloomH = height / BLOOM_DOWNSCALE;

  auto createRT = [](UINT w, UINT h, const wchar_t* name) -> com_ptr<ID3D12Resource>
  {
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto desc = CD3DX12_RESOURCE_DESC::Tex2D(Core::GetBackBufferFormat(), w, h, 1, 1);
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = Core::GetBackBufferFormat();

    com_ptr<ID3D12Resource> resource;
    check_hresult(Core::GetD3DDevice()->CreateCommittedResource(
      &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue,
      IID_GRAPHICS_PPV_ARGS(resource)));
    SetName(resource.get(), name);
    return resource;
  };

  m_brightPassRT = createRT(bloomW, bloomH, L"BloomBrightPass");
  m_blurRT[0] = createRT(bloomW, bloomH, L"BloomBlurRT0");
  m_blurRT[1] = createRT(bloomW, bloomH, L"BloomBlurRT1");

  // Allocate RTVs from the CPU-visible RTV descriptor allocator
  m_brightPassRTV = Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  Core::GetD3DDevice()->CreateRenderTargetView(m_brightPassRT.get(), nullptr, m_brightPassRTV);

  for (int i = 0; i < 2; ++i)
  {
    m_blurRTV[i] = Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    Core::GetD3DDevice()->CreateRenderTargetView(m_blurRT[i].get(), nullptr, m_blurRTV[i]);
  }
}

void PostProcess::ApplyBloom(ID3D12GraphicsCommandList* cmdList,
  ConstantBufferAllocator& cbAlloc,
  ShaderVisibleHeap& srvHeap,
  ID3D12Resource* sceneRT,
  float threshold)
{
  const UINT bloomW = m_width / BLOOM_DOWNSCALE;
  const UINT bloomH = m_height / BLOOM_DOWNSCALE;

  // Bind the shader-visible descriptor heap
  ID3D12DescriptorHeap* heaps[] = { srvHeap.GetHeap() };
  cmdList->SetDescriptorHeaps(1, heaps);

  cmdList->SetGraphicsRootSignature(m_rootSignature.get());
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  auto device = Core::GetD3DDevice();

  // Helper: create SRV in shader-visible heap
  auto makeSRV = [&](ID3D12Resource* resource) -> DescriptorHandle
  {
    auto handle = srvHeap.Allocate(1);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = Core::GetBackBufferFormat();
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(resource, &srvDesc, handle);
    return handle;
  };

  // ── Pass 1: Bright-pass extract ──────────────────────────────────────
  {
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      sceneRT, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmdList->ResourceBarrier(1, &barrier);

    auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
      m_brightPassRT.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->ResourceBarrier(1, &barrier2);

    struct { float Threshold; float _Pad[3]; } data = { threshold, {} };
    auto cb = cbAlloc.Allocate(sizeof(data));
    memcpy(cb.CpuAddress, &data, sizeof(data));

    auto sceneSRV = makeSRV(sceneRT);

    D3D12_VIEWPORT vp = { 0, 0, static_cast<float>(bloomW), static_cast<float>(bloomH), 0, 1 };
    D3D12_RECT scissor = { 0, 0, static_cast<LONG>(bloomW), static_cast<LONG>(bloomH) };
    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &scissor);
    cmdList->OMSetRenderTargets(1, &m_brightPassRTV, FALSE, nullptr);

    cmdList->SetPipelineState(m_extractPSO.get());
    cmdList->SetGraphicsRootConstantBufferView(0, cb.GpuAddress);
    cmdList->SetGraphicsRootDescriptorTable(1, sceneSRV);
    cmdList->DrawInstanced(3, 1, 0, 0);
  }

  // ── Pass 2: Gaussian blur (horizontal then vertical) ──────────────
  {
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      m_brightPassRT.get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmdList->ResourceBarrier(1, &barrier);

    cmdList->SetPipelineState(m_blurPSO.get());

    // Horizontal blur
    {
      auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
        m_blurRT[0].get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
      cmdList->ResourceBarrier(1, &barrier2);

      struct { float TexelX, TexelY, _Pad0, _Pad1; } data = { 1.0f / bloomW, 0, 0, 0 };
      auto cb = cbAlloc.Allocate(sizeof(data));
      memcpy(cb.CpuAddress, &data, sizeof(data));

      auto srcSRV = makeSRV(m_brightPassRT.get());
      cmdList->OMSetRenderTargets(1, &m_blurRTV[0], FALSE, nullptr);
      cmdList->SetGraphicsRootConstantBufferView(0, cb.GpuAddress);
      cmdList->SetGraphicsRootDescriptorTable(1, srcSRV);
      cmdList->DrawInstanced(3, 1, 0, 0);

      auto barrier3 = CD3DX12_RESOURCE_BARRIER::Transition(
        m_blurRT[0].get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      cmdList->ResourceBarrier(1, &barrier3);
    }

    // Vertical blur
    {
      auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
        m_blurRT[1].get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
      cmdList->ResourceBarrier(1, &barrier2);

      struct { float TexelX, TexelY, _Pad0, _Pad1; } data = { 0, 1.0f / bloomH, 0, 0 };
      auto cb = cbAlloc.Allocate(sizeof(data));
      memcpy(cb.CpuAddress, &data, sizeof(data));

      auto srcSRV = makeSRV(m_blurRT[0].get());
      cmdList->OMSetRenderTargets(1, &m_blurRTV[1], FALSE, nullptr);
      cmdList->SetGraphicsRootConstantBufferView(0, cb.GpuAddress);
      cmdList->SetGraphicsRootDescriptorTable(1, srcSRV);
      cmdList->DrawInstanced(3, 1, 0, 0);

      auto barrier3 = CD3DX12_RESOURCE_BARRIER::Transition(
        m_blurRT[1].get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      cmdList->ResourceBarrier(1, &barrier3);
    }
  }

  // ── Pass 3: Composite bloom back onto scene ──────────────────────────
  {
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      sceneRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->ResourceBarrier(1, &barrier);

    // Restore full-resolution viewport
    auto vp = Core::GetScreenViewport();
    auto scissor = Core::GetScissorRect();
    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &scissor);

    auto rtvHandle = Core::GetRenderTargetView();
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Allocate 2 SRVs for scene + bloom (contiguous for descriptor table)
    auto srvPair = srvHeap.Allocate(2);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = Core::GetBackBufferFormat();
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    auto cpu0 = static_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(srvPair);
    device->CreateShaderResourceView(sceneRT, &srvDesc, cpu0);

    D3D12_CPU_DESCRIPTOR_HANDLE cpu1 = cpu0;
    cpu1.ptr += srvHeap.GetDescriptorSize();
    device->CreateShaderResourceView(m_blurRT[1].get(), &srvDesc, cpu1);

    struct { float _Pad[4]; } data = {};
    auto cb = cbAlloc.Allocate(sizeof(data));
    memcpy(cb.CpuAddress, &data, sizeof(data));

    cmdList->SetPipelineState(m_compositePSO.get());
    cmdList->SetGraphicsRootConstantBufferView(0, cb.GpuAddress);
    cmdList->SetGraphicsRootDescriptorTable(1, srvPair);
    cmdList->DrawInstanced(3, 1, 0, 0);
  }
}
