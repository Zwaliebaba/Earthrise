#include "pch.h"
#include "SunBillboard.h"
#include "GpuResourceManager.h"
#include "DdsLoader.h"
#include "CompiledShaders/BillboardVS.h"
#include "CompiledShaders/BillboardPS.h"

using namespace Neuron::Graphics;

void SunBillboard::Initialize(ShaderVisibleHeap& _srvHeap)
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

  // Non-shader-visible SRV for texture
  m_glowSRV_CPU = Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = m_glowTexture->GetDesc().Format;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Texture2D.MipLevels = 1;
  Core::GetD3DDevice()->CreateShaderResourceView(m_glowTexture.get(), &srvDesc, m_glowSRV_CPU);

  // Allocate a persistent slot in the shader-visible heap and copy the SRV once.
  auto persistentHandle = _srvHeap.AllocatePersistent(1);
  Core::GetD3DDevice()->CopyDescriptorsSimple(1,
    static_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(persistentHandle),
    m_glowSRV_CPU,
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  m_glowSRV_GPU = persistentHandle;

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

  m_pso = PsoBuilder()
    .WithRootSignature(m_rootSignature.get())
    .WithVS(g_pBillboardVS, sizeof(g_pBillboardVS))
    .WithPS(g_pBillboardPS, sizeof(g_pBillboardPS))
    .NoCull()
    .AdditiveBlend()
    .DepthReadOnly()
    .Build();
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

  // Bind the shader-visible descriptor heap
  ID3D12DescriptorHeap* heaps[] = { _srvHeap.GetHeap() };
  _cmdList->SetDescriptorHeaps(1, heaps);

  _cmdList->SetPipelineState(m_pso.get());
  _cmdList->SetGraphicsRootSignature(m_rootSignature.get());
  _cmdList->SetGraphicsRootConstantBufferView(0, alloc.GpuAddress);
  _cmdList->SetGraphicsRootDescriptorTable(1, m_glowSRV_GPU);
  _cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  _cmdList->DrawInstanced(6, 1, 0, 0);
}
