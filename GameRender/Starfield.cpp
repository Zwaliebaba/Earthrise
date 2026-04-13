#include "pch.h"
#include "Starfield.h"
#include "GpuResourceManager.h"
#include "CompiledShaders/StarfieldVS.h"
#include "CompiledShaders/StarfieldPS.h"

using namespace Neuron::Graphics;

namespace
{
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

  // Input layout
  D3D12_INPUT_ELEMENT_DESC inputLayout[] =
  {
    { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "BRIGHTNESS", 0, DXGI_FORMAT_R32_FLOAT,       0, sizeof(XMFLOAT3),     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };

  // PSO — render as points, reverse-Z depth, no depth write (background)
  m_pso = PsoBuilder()
    .WithRootSignature(m_rootSignature.get())
    .WithVS(g_pStarfieldVS, sizeof(g_pStarfieldVS))
    .WithPS(g_pStarfieldPS, sizeof(g_pStarfieldPS))
    .WithInputLayout(inputLayout, _countof(inputLayout))
    .DepthReadOnly()
    .Topology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT)
    .Build();
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
