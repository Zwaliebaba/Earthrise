#include "pch.h"
#include "SunBillboard.h"
#include "GpuResourceManager.h"
#include "DdsLoader.h"
#include "CompiledShaders/BillboardVS.h"
#include "CompiledShaders/BillboardPS.h"

using namespace Neuron::Graphics;

namespace
{
  constexpr int   MAX_BLOBS      = 50;
  constexpr int   MAX_STARS      = 10;
  constexpr float BLOB_SPREAD    = 0.035f;   // multiplied by _visualRadius
  constexpr float BLOB_Y_SCALE   = 1.35f;
  constexpr float BLOB_BASE_SIZE = 0.2f;
  constexpr float BLOB_MIN_SIZE  = 0.05f;
  constexpr float STAR_SIZE_SCALE = 0.25f;
}

static com_ptr<ID3D12Resource> LoadTexture(
  const wchar_t* _fileName,
  const wchar_t* _debugName,
  ShaderVisibleHeap& _srvHeap,
  D3D12_CPU_DESCRIPTOR_HANDLE& _cpuSrv,
  D3D12_GPU_DESCRIPTOR_HANDLE& _gpuSrv)
{
  std::wstring texPath = FileSys::GetHomeDirectory();
  texPath += L"Textures\\";
  texPath += _fileName;
  CpuImage image = DdsLoader::LoadFromFile(texPath);
  if (image.Pixels.empty())
  {
    Neuron::DebugTrace(L"SunBillboard: {} not found\n", _fileName);
    return nullptr;
  }

  auto texture = GpuResourceManager::CreateStaticTexture(image, _debugName);

  _cpuSrv = Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = texture->GetDesc().Format;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Texture2D.MipLevels = 1;
  Core::GetD3DDevice()->CreateShaderResourceView(texture.get(), &srvDesc, _cpuSrv);

  auto persistentHandle = _srvHeap.AllocatePersistent(1);
  Core::GetD3DDevice()->CopyDescriptorsSimple(1,
    static_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(persistentHandle),
    _cpuSrv,
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  _gpuSrv = persistentHandle;

  return texture;
}

void SunBillboard::Initialize(ShaderVisibleHeap& _srvHeap)
{
  m_cloudyGlowTexture = LoadTexture(L"CloudyGlow.dds", L"CloudyGlowTexture", _srvHeap,
    m_cloudyGlowSRV_CPU, m_cloudyGlowSRV_GPU);
  m_starburstTexture = LoadTexture(L"Starburst.dds", L"StarburstTexture", _srvHeap,
    m_starburstSRV_CPU, m_starburstSRV_GPU);

  if (!m_cloudyGlowTexture && !m_starburstTexture)
    return;

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
  GXMVECTOR _color,
  float _gameTime)
{
  if (!m_pso) return;

  // Fade-in over 10 seconds
  m_fadeTimer += Neuron::Timer::Core::GetElapsedSeconds();
  float alpha = std::min(m_fadeTimer * 0.1f, 1.0f);

  float timeIndex = _gameTime * 2.0f;

  // Pre-compute camera-invariant CB fields
  XMFLOAT4X4 viewProj;
  XMStoreFloat4x4(&viewProj, XMMatrixTranspose(_camera.GetViewProjectionMatrix()));
  XMFLOAT3 camRight, camUp;
  XMStoreFloat3(&camRight, _camera.GetRight());
  XMStoreFloat3(&camUp, _camera.GetUp());

  XMFLOAT3 center;
  XMStoreFloat3(&center, _rebasedPosition);

  XMFLOAT4 baseColor;
  XMStoreFloat4(&baseColor, _color);

  float spread    = _visualRadius * BLOB_SPREAD;
  float yScale    = _visualRadius * BLOB_Y_SCALE;
  float baseSize  = _visualRadius * BLOB_BASE_SIZE;
  float minSize   = _visualRadius * BLOB_MIN_SIZE;
  float starScale = _visualRadius * STAR_SIZE_SCALE;

  // Bind pipeline state once
  ID3D12DescriptorHeap* heaps[] = { _srvHeap.GetHeap() };
  _cmdList->SetDescriptorHeaps(1, heaps);
  _cmdList->SetPipelineState(m_pso.get());
  _cmdList->SetGraphicsRootSignature(m_rootSignature.get());
  _cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // --- Pass A: CloudyGlow blobs ---
  if (m_cloudyGlowTexture)
  {
    _cmdList->SetGraphicsRootDescriptorTable(1, m_cloudyGlowSRV_GPU);

    for (int i = 0; i < MAX_BLOBS; ++i)
    {
      XMFLOAT3 blobPos = center;
      blobPos.x += sinf(timeIndex + i) * i * spread;
      blobPos.y += fabsf(cosf(timeIndex + i) * cosf(i * 20.0f)) * yScale;
      blobPos.z += cosf(timeIndex + i) * i * spread;

      float radius = std::max(baseSize * sinf(timeIndex + i * 13.0f), minSize);

      BillboardConstants cb{};
      cb.ViewProjection = viewProj;
      cb.CameraRight = camRight;
      cb.CameraUp = camUp;
      cb.Center = blobPos;
      cb.Radius = radius;
      cb.Color = { 0.6f * baseColor.x, 0.2f * baseColor.y,
                   0.1f * baseColor.z, alpha * baseColor.w };

      auto alloc = _cbAlloc.Allocate<BillboardConstants>();
      memcpy(alloc.CpuAddress, &cb, sizeof(cb));
      _cmdList->SetGraphicsRootConstantBufferView(0, alloc.GpuAddress);
      _cmdList->DrawInstanced(6, 1, 0, 0);
    }
  }

  // --- Pass B: Starburst rays ---
  if (m_starburstTexture)
  {
    _cmdList->SetGraphicsRootDescriptorTable(1, m_starburstSRV_GPU);

    for (int i = 1; i < MAX_STARS; ++i)
    {
      XMFLOAT3 starPos = center;
      starPos.x += sinf(timeIndex + i) * i * spread;
      starPos.y += fabsf(cosf(timeIndex + i) * cosf(i * 20.0f)) * yScale;
      starPos.z += cosf(timeIndex + i) * i * spread;

      float radius = i * starScale;

      BillboardConstants cb{};
      cb.ViewProjection = viewProj;
      cb.CameraRight = camRight;
      cb.CameraUp = camUp;
      cb.Center = starPos;
      cb.Radius = radius;
      cb.Color = { 1.0f * baseColor.x, 0.4f * baseColor.y,
                   0.2f * baseColor.z, alpha * baseColor.w };

      auto alloc = _cbAlloc.Allocate<BillboardConstants>();
      memcpy(alloc.CpuAddress, &cb, sizeof(cb));
      _cmdList->SetGraphicsRootConstantBufferView(0, alloc.GpuAddress);
      _cmdList->DrawInstanced(6, 1, 0, 0);
    }
  }
}
