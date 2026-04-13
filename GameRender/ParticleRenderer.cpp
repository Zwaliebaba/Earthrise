#include "pch.h"
#include "ParticleRenderer.h"
#include "PipelineHelpers.h"
#include "CompiledShaders/ParticleVS.h"
#include "CompiledShaders/ParticlePS.h"

using namespace Neuron::Graphics;

// GPU-side per-particle data matching the StructuredBuffer in ParticleVS.hlsl.
struct GpuParticleData
{
  XMFLOAT3 Position;  // Origin-rebased
  float    Size;
  XMFLOAT4 Color;
};
static_assert(sizeof(GpuParticleData) == 32);

// Frame constants for the particle pipeline (matches cbuffer b0 in ParticleVS.hlsl).
struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) ParticleFrameConstants
{
  XMFLOAT4X4 ViewProjection;
  XMFLOAT3   CameraRight;
  float      _Pad0 = 0;
  XMFLOAT3   CameraUp;
  float      _Pad1 = 0;
};

void ParticleRenderer::Initialize()
{
  // Root signature: b0 = root CBV (frame constants), t0 = SRV descriptor table (particle buffer)
  CD3DX12_ROOT_PARAMETER rootParams[2];
  rootParams[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

  CD3DX12_DESCRIPTOR_RANGE srvRange;
  srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  rootParams[1].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_VERTEX);

  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
  rootSigDesc.Init(_countof(rootParams), rootParams, 0, nullptr,
    D3D12_ROOT_SIGNATURE_FLAG_NONE);

  m_rootSignature = PipelineHelpers::CreateRootSignature(rootSigDesc);
  SetName(m_rootSignature.get(), L"ParticleRootSig");

  // PSO — no input layout (SV_VertexID + SV_InstanceID), additive blend, no depth write
  m_pso = PsoBuilder()
    .WithRootSignature(m_rootSignature.get())
    .WithVS(g_pParticleVS, sizeof(g_pParticleVS))
    .WithPS(g_pParticlePS, sizeof(g_pParticlePS))
    .NoCull()
    .AdditiveBlend()
    .DepthReadOnly()
    .Build();
  SetName(m_pso.get(), L"ParticlePSO");
}

void ParticleRenderer::Render(ID3D12GraphicsCommandList* _cmdList,
                               UploadHeap& _uploadHeap,
                               ConstantBufferAllocator& _cbAlloc,
                               ShaderVisibleHeap& _srvHeap,
                               const Camera& _camera,
                               const ParticleSystem& _particles)
{
  const auto& particles = _particles.GetParticles();
  if (particles.empty()) return;

  const uint32_t count = static_cast<uint32_t>(particles.size());
  XMVECTOR camPos = _camera.GetPosition();

  // Upload per-particle data as a structured buffer.
  const size_t bufferSize = count * sizeof(GpuParticleData);
  auto bufferAlloc = _uploadHeap.Allocate(bufferSize, sizeof(GpuParticleData));

  auto* dst = static_cast<GpuParticleData*>(bufferAlloc.CpuAddress);
  for (uint32_t i = 0; i < count; ++i)
  {
    const auto& p = particles[i];
    // Origin-rebased position.
    XMVECTOR worldPos = XMLoadFloat3(&p.Position);
    XMVECTOR rebased  = XMVectorSubtract(worldPos, XMVectorSetW(camPos, 0.0f));
    XMStoreFloat3(&dst[i].Position, rebased);
    dst[i].Size = p.Size;
    dst[i].Color = p.Color;
  }

  // Create SRV for the structured buffer in the shader-visible heap.
  // The allocation lives at an offset within the upload heap resource.
  auto srvHandle = _srvHeap.Allocate(1);

  D3D12_GPU_VIRTUAL_ADDRESS resourceBase = _uploadHeap.GetResource()->GetGPUVirtualAddress();
  uint64_t byteOffset = bufferAlloc.GpuAddress - resourceBase;

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format = DXGI_FORMAT_UNKNOWN;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Buffer.FirstElement = byteOffset / sizeof(GpuParticleData);
  srvDesc.Buffer.NumElements = count;
  srvDesc.Buffer.StructureByteStride = sizeof(GpuParticleData);

  Core::GetD3DDevice()->CreateShaderResourceView(
    _uploadHeap.GetResource(), &srvDesc, srvHandle);

  // Upload frame constants.
  ParticleFrameConstants fc{};
  XMStoreFloat4x4(&fc.ViewProjection, XMMatrixTranspose(_camera.GetViewProjectionMatrix()));
  XMStoreFloat3(&fc.CameraRight, _camera.GetRight());
  XMStoreFloat3(&fc.CameraUp, _camera.GetUp());
  auto cbAlloc = _cbAlloc.Allocate<ParticleFrameConstants>();
  memcpy(cbAlloc.CpuAddress, &fc, sizeof(ParticleFrameConstants));

  // Bind pipeline state.
  ID3D12DescriptorHeap* heaps[] = { _srvHeap.GetHeap() };
  _cmdList->SetDescriptorHeaps(1, heaps);

  _cmdList->SetPipelineState(m_pso.get());
  _cmdList->SetGraphicsRootSignature(m_rootSignature.get());
  _cmdList->SetGraphicsRootConstantBufferView(0, cbAlloc.GpuAddress);
  _cmdList->SetGraphicsRootDescriptorTable(1, srvHandle);
  _cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // Single draw call: 6 vertices per quad × N particle instances.
  _cmdList->DrawInstanced(6, count, 0, 0);
}
