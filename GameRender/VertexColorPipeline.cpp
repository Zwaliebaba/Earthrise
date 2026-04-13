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

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
  psoDesc.pRootSignature = m_rootSignature.get();
  psoDesc.VS = { g_pVertexColorVS, sizeof(g_pVertexColorVS) };
  psoDesc.PS = { g_pVertexColorPS, sizeof(g_pVertexColorPS) };
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
