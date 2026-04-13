#include "pch.h"
#include "PostProcess.h"
#include "CompiledShaders/FullscreenVS.h"
#include "CompiledShaders/BloomExtractPS.h"
#include "CompiledShaders/BloomBlurPS.h"
#include "CompiledShaders/BloomCompositePS.h"
#include "CompiledShaders/BloomAdditivePS.h"

using namespace Neuron::Graphics;

void PostProcess::Initialize(UINT width, UINT height)
{
  // Root signature: 1 CBV (b0) + 1 descriptor table (SRVs t0-t1) + 1 static sampler
  CD3DX12_DESCRIPTOR_RANGE srvRange;
  srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0); // t0, t1

  CD3DX12_ROOT_PARAMETER rootParams[2];
  rootParams[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
  rootParams[1].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

  CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
  rootSigDesc.Init(_countof(rootParams), rootParams, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  m_rootSignature = PipelineHelpers::CreateRootSignature(rootSigDesc);
  SetName(m_rootSignature.get(), L"PostProcessRootSig");

  // Common PSO state (no depth, full-screen triangle, no input layout)
  auto makePSO = [&](const void* psBytecode, size_t psSize) -> com_ptr<ID3D12PipelineState>
  {
    return PsoBuilder()
      .WithRootSignature(m_rootSignature.get())
      .WithVS(g_pFullscreenVS, sizeof(g_pFullscreenVS))
      .WithPS(psBytecode, psSize)
      .NoCull()
      .NoDepth()
      .Build();
  };

  m_extractPSO = makePSO(g_pBloomExtractPS, sizeof(g_pBloomExtractPS));
  SetName(m_extractPSO.get(), L"BloomExtractPSO");

  m_blurPSO = makePSO(g_pBloomBlurPS, sizeof(g_pBloomBlurPS));
  SetName(m_blurPSO.get(), L"BloomBlurPSO");

  m_compositePSO = makePSO(g_pBloomCompositePS, sizeof(g_pBloomCompositePS));
  SetName(m_compositePSO.get(), L"BloomCompositePSO");

  // Additive composite PSO — same as composite but with additive blending, 1 SRV only
  m_compositeAdditivePSO = PsoBuilder()
    .WithRootSignature(m_rootSignature.get())
    .WithVS(g_pFullscreenVS, sizeof(g_pFullscreenVS))
    .WithPS(g_pBloomAdditivePS, sizeof(g_pBloomAdditivePS))
    .NoCull()
    .PureAdditiveBlend()
    .NoDepth()
    .Build();
  SetName(m_compositeAdditivePSO.get(), L"BloomCompositeAdditivePSO");

  CreateResources(width, height);
}

void PostProcess::OnResize(UINT width, UINT height)
{
  if (width == m_width && height == m_height)
    return;
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
    check_hresult(Core::GetD3DDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
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

  // Create CPU-side SRVs for bloom RTs (copied to shader-visible heap per-frame)
  auto createSRV = [device = Core::GetD3DDevice()](ID3D12Resource* resource) -> D3D12_CPU_DESCRIPTOR_HANDLE
  {
    auto handle = Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = Core::GetBackBufferFormat();
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(resource, &srvDesc, handle);
    return handle;
  };

  m_brightPassSRV_CPU = createSRV(m_brightPassRT.get());
  m_blurSRV_CPU[0] = createSRV(m_blurRT[0].get());
  m_blurSRV_CPU[1] = createSRV(m_blurRT[1].get());
}

void PostProcess::ApplyBloom(ID3D12GraphicsCommandList* cmdList, ConstantBufferAllocator& cbAlloc, ShaderVisibleHeap& srvHeap,
                             ID3D12Resource* sceneRT, float threshold)
{
  const UINT bloomW = m_width / BLOOM_DOWNSCALE;
  const UINT bloomH = m_height / BLOOM_DOWNSCALE;

  // Bind the shader-visible descriptor heap
  ID3D12DescriptorHeap* heaps[] = {srvHeap.GetHeap()};
  cmdList->SetDescriptorHeaps(1, heaps);

  cmdList->SetGraphicsRootSignature(m_rootSignature.get());
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  auto device = Core::GetD3DDevice();

  // Helper: create SRV for the scene RT (back buffer, changes per frame)
  auto makeSceneSRV = [&]() -> DescriptorHandle
  {
    auto handle = srvHeap.Allocate(1);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = Core::GetBackBufferFormat();
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(sceneRT, &srvDesc, handle);
    return handle;
  };

  // Helper: copy a cached CPU-side SRV into the shader-visible heap
  auto copySRV = [&](D3D12_CPU_DESCRIPTOR_HANDLE cpuSRV) -> DescriptorHandle
  {
    auto handle = srvHeap.Allocate(1);
    device->CopyDescriptorsSimple(1,
      static_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(handle), cpuSRV,
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    return handle;
  };

  // ── Pass 1: Bright-pass extract ──────────────────────────────────────
  {
    D3D12_RESOURCE_BARRIER barriers[] = {
      CD3DX12_RESOURCE_BARRIER::Transition(sceneRT, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
      CD3DX12_RESOURCE_BARRIER::Transition(m_brightPassRT.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                           D3D12_RESOURCE_STATE_RENDER_TARGET),
    };
    cmdList->ResourceBarrier(_countof(barriers), barriers);

    struct
    {
      float Threshold;
      float _Pad[3];
    } data = {threshold, {}};
    auto cb = cbAlloc.Allocate(sizeof(data));
    memcpy(cb.CpuAddress, &data, sizeof(data));

    auto sceneSRV = makeSceneSRV();

    D3D12_VIEWPORT vp = {0, 0, static_cast<float>(bloomW), static_cast<float>(bloomH), 0, 1};
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(bloomW), static_cast<LONG>(bloomH)};
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
    D3D12_RESOURCE_BARRIER barriers[] = {
      CD3DX12_RESOURCE_BARRIER::Transition(m_brightPassRT.get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
      CD3DX12_RESOURCE_BARRIER::Transition(m_blurRT[0].get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                           D3D12_RESOURCE_STATE_RENDER_TARGET),
    };
    cmdList->ResourceBarrier(_countof(barriers), barriers);

    cmdList->SetPipelineState(m_blurPSO.get());

    // Horizontal blur
    {
      struct
      {
        float TexelX, TexelY, _Pad0, _Pad1;
      } data = {1.0f / bloomW, 0, 0, 0};
      auto cb = cbAlloc.Allocate(sizeof(data));
      memcpy(cb.CpuAddress, &data, sizeof(data));

      auto srcSRV = copySRV(m_brightPassSRV_CPU);
      cmdList->OMSetRenderTargets(1, &m_blurRTV[0], FALSE, nullptr);
      cmdList->SetGraphicsRootConstantBufferView(0, cb.GpuAddress);
      cmdList->SetGraphicsRootDescriptorTable(1, srcSRV);
      cmdList->DrawInstanced(3, 1, 0, 0);
    }

    // V-blur entry: blurRT[0] → SRV, blurRT[1] → RT
    {
      D3D12_RESOURCE_BARRIER blurBarriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(m_blurRT[0].get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                             D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(m_blurRT[1].get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                             D3D12_RESOURCE_STATE_RENDER_TARGET),
      };
      cmdList->ResourceBarrier(_countof(blurBarriers), blurBarriers);

      struct
      {
        float TexelX, TexelY, _Pad0, _Pad1;
      } data = {0, 1.0f / bloomH, 0, 0};
      auto cb = cbAlloc.Allocate(sizeof(data));
      memcpy(cb.CpuAddress, &data, sizeof(data));

      auto srcSRV = copySRV(m_blurSRV_CPU[0]);
      cmdList->OMSetRenderTargets(1, &m_blurRTV[1], FALSE, nullptr);
      cmdList->SetGraphicsRootConstantBufferView(0, cb.GpuAddress);
      cmdList->SetGraphicsRootDescriptorTable(1, srcSRV);
      cmdList->DrawInstanced(3, 1, 0, 0);

      auto exitBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_blurRT[1].get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                              D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      cmdList->ResourceBarrier(1, &exitBarrier);
    }
  }

  // ── Pass 3: Composite bloom back onto scene ──────────────────────────
  {
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(sceneRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                        D3D12_RESOURCE_STATE_RENDER_TARGET);
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
    device->CopyDescriptorsSimple(1, cpu1, m_blurSRV_CPU[1],
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    struct
    {
      float _Pad[4];
    } data = {};
    auto cb = cbAlloc.Allocate(sizeof(data));
    memcpy(cb.CpuAddress, &data, sizeof(data));

    cmdList->SetPipelineState(m_compositePSO.get());
    cmdList->SetGraphicsRootConstantBufferView(0, cb.GpuAddress);
    cmdList->SetGraphicsRootDescriptorTable(1, srvPair);
    cmdList->DrawInstanced(3, 1, 0, 0);
  }
}

void PostProcess::ApplyBloomAdditive(ID3D12GraphicsCommandList* cmdList, ConstantBufferAllocator& cbAlloc, ShaderVisibleHeap& srvHeap,
                                     ID3D12Resource* sceneRT, float threshold) const
{
  auto* device = Core::GetD3DDevice();
  const float bloomW = static_cast<float>(m_width / BLOOM_DOWNSCALE);
  const float bloomH = static_cast<float>(m_height / BLOOM_DOWNSCALE);

  // Bind the shader-visible descriptor heap
  ID3D12DescriptorHeap* heaps[] = {srvHeap.GetHeap()};
  cmdList->SetDescriptorHeaps(1, heaps);

  // Bind root signature for all passes
  cmdList->SetGraphicsRootSignature(m_rootSignature.get());
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // Helper: create SRV for the scene RT (back buffer, changes per frame)
  auto makeSceneSRV = [&]() -> DescriptorHandle
  {
    auto handle = srvHeap.Allocate(1);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = Core::GetBackBufferFormat();
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(sceneRT, &srvDesc, handle);
    return handle;
  };

  // Helper: copy a cached CPU-side SRV into the shader-visible heap
  auto copySRV = [&](D3D12_CPU_DESCRIPTOR_HANDLE cpuSRV) -> DescriptorHandle
  {
    auto handle = srvHeap.Allocate(1);
    device->CopyDescriptorsSimple(1,
      static_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(handle), cpuSRV,
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    return handle;
  };

  // ── Pass 1: Bright-pass extract ──────────────────────────────────────
  {
    D3D12_RESOURCE_BARRIER barriers[] = {
      CD3DX12_RESOURCE_BARRIER::Transition(sceneRT, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
      CD3DX12_RESOURCE_BARRIER::Transition(m_brightPassRT.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                           D3D12_RESOURCE_STATE_RENDER_TARGET),
    };
    cmdList->ResourceBarrier(_countof(barriers), barriers);

    struct
    {
      float Threshold;
      float _Pad[3];
    } data = {threshold, {}};
    auto cb = cbAlloc.Allocate(sizeof(data));
    memcpy(cb.CpuAddress, &data, sizeof(data));

    auto sceneSRV = makeSceneSRV();

    D3D12_VIEWPORT vp = {0, 0, bloomW, bloomH, 0, 1};
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(bloomW), static_cast<LONG>(bloomH)};
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
    D3D12_RESOURCE_BARRIER barriers[] = {
      CD3DX12_RESOURCE_BARRIER::Transition(m_brightPassRT.get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
      CD3DX12_RESOURCE_BARRIER::Transition(m_blurRT[0].get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                           D3D12_RESOURCE_STATE_RENDER_TARGET),
    };
    cmdList->ResourceBarrier(_countof(barriers), barriers);

    cmdList->SetPipelineState(m_blurPSO.get());

    // Horizontal blur
    {
      struct
      {
        float TexelX, TexelY, _Pad0, _Pad1;
      } data = {1.0f / bloomW, 0, 0, 0};
      auto cb = cbAlloc.Allocate(sizeof(data));
      memcpy(cb.CpuAddress, &data, sizeof(data));

      auto srcSRV = copySRV(m_brightPassSRV_CPU);
      cmdList->OMSetRenderTargets(1, &m_blurRTV[0], FALSE, nullptr);
      cmdList->SetGraphicsRootConstantBufferView(0, cb.GpuAddress);
      cmdList->SetGraphicsRootDescriptorTable(1, srcSRV);
      cmdList->DrawInstanced(3, 1, 0, 0);
    }

    // V-blur entry: blurRT[0] → SRV, blurRT[1] → RT
    {
      D3D12_RESOURCE_BARRIER blurBarriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(m_blurRT[0].get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                             D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(m_blurRT[1].get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                             D3D12_RESOURCE_STATE_RENDER_TARGET),
      };
      cmdList->ResourceBarrier(_countof(blurBarriers), blurBarriers);

      struct
      {
        float TexelX, TexelY, _Pad0, _Pad1;
      } data = {0, 1.0f / bloomH, 0, 0};
      auto cb = cbAlloc.Allocate(sizeof(data));
      memcpy(cb.CpuAddress, &data, sizeof(data));

      auto srcSRV = copySRV(m_blurSRV_CPU[0]);
      cmdList->OMSetRenderTargets(1, &m_blurRTV[1], FALSE, nullptr);
      cmdList->SetGraphicsRootConstantBufferView(0, cb.GpuAddress);
      cmdList->SetGraphicsRootDescriptorTable(1, srcSRV);
      cmdList->DrawInstanced(3, 1, 0, 0);

      auto exitBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_blurRT[1].get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                              D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      cmdList->ResourceBarrier(1, &exitBarrier);
    }
  }

  // ── Pass 3: Additive composite — bloom only, blended onto back buffer ──
  {
    // Transition scene RT back to RENDER_TARGET (it's the back buffer)
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(sceneRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                        D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->ResourceBarrier(1, &barrier);

    // Restore full-resolution viewport
    auto vp = Core::GetScreenViewport();
    auto scissor = Core::GetScissorRect();
    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &scissor);

    auto rtvHandle = Core::GetRenderTargetView();
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Single SRV for bloom texture
    auto bloomSRV = copySRV(m_blurSRV_CPU[1]);

    struct
    {
      float _Pad[4];
    } data = {};
    auto cb = cbAlloc.Allocate(sizeof(data));
    memcpy(cb.CpuAddress, &data, sizeof(data));

    cmdList->SetPipelineState(m_compositeAdditivePSO.get());
    cmdList->SetGraphicsRootConstantBufferView(0, cb.GpuAddress);
    cmdList->SetGraphicsRootDescriptorTable(1, bloomSRV);
    cmdList->DrawInstanced(3, 1, 0, 0);
  }
}
