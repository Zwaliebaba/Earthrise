#include "pch.h"
#include "FlatColorPipeline.h"
#include "CompiledShaders/FlatColorVS.h"
#include "CompiledShaders/FlatColorPS.h"

using namespace Neuron::Graphics;

void FlatColorPipeline::Initialize()
{
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

  m_pso = PsoBuilder()
    .WithRootSignature(m_rootSignature.get())
    .WithVS(g_pFlatColorVS, sizeof(g_pFlatColorVS))
    .WithPS(g_pFlatColorPS, sizeof(g_pFlatColorPS))
    .WithInputLayout(inputLayout, _countof(inputLayout))
    .FrontCCW()
    .DepthReverseZ()
    .Build();
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
