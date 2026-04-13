#include "pch.h"
#include "VertexColorPipeline.h"
#include "CompiledShaders/VertexColorVS.h"
#include "CompiledShaders/VertexColorPS.h"

using namespace Neuron::Graphics;

void VertexColorPipeline::Initialize()
{
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

  m_pso = PsoBuilder()
    .WithRootSignature(m_rootSignature.get())
    .WithVS(g_pVertexColorVS, sizeof(g_pVertexColorVS))
    .WithPS(g_pVertexColorPS, sizeof(g_pVertexColorPS))
    .WithInputLayout(inputLayout, _countof(inputLayout))
    .FrontCCW()
    .DepthReverseZ()
    .Build();
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
