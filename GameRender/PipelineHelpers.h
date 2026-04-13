#pragma once

namespace Neuron::Graphics
{
  // Utility functions to build root signatures and PSOs.
  namespace PipelineHelpers
  {
    // Create a root signature from a serialized blob.
    [[nodiscard]] com_ptr<ID3D12RootSignature> CreateRootSignature(
      const D3D12_ROOT_SIGNATURE_DESC& desc);

    // Create a graphics PSO.
    [[nodiscard]] com_ptr<ID3D12PipelineState> CreateGraphicsPSO(
      const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);
  }

  // Fluent builder for D3D12_GRAPHICS_PIPELINE_STATE_DESC.
  // Pre-fills common defaults (SampleMask, NumRenderTargets, RTVFormat, SampleDesc).
  struct PsoBuilder
  {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc = {};

    PsoBuilder()
    {
      Desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
      Desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
      Desc.SampleMask = UINT_MAX;
      Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
      Desc.NumRenderTargets = 1;
      Desc.RTVFormats[0] = Core::GetBackBufferFormat();
      Desc.SampleDesc.Count = 1;
    }

    PsoBuilder& WithRootSignature(ID3D12RootSignature* rs) { Desc.pRootSignature = rs; return *this; }
    PsoBuilder& WithVS(const void* bytecode, size_t size) { Desc.VS = {bytecode, size}; return *this; }
    PsoBuilder& WithPS(const void* bytecode, size_t size) { Desc.PS = {bytecode, size}; return *this; }
    PsoBuilder& WithInputLayout(const D3D12_INPUT_ELEMENT_DESC* elems, UINT count) { Desc.InputLayout = {elems, count}; return *this; }

    PsoBuilder& NoCull() { Desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; return *this; }
    PsoBuilder& FrontCCW() { Desc.RasterizerState.FrontCounterClockwise = TRUE; return *this; }

    PsoBuilder& AlphaBlend()
    {
      auto& rt = Desc.BlendState.RenderTarget[0];
      rt.BlendEnable = TRUE;
      rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
      rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
      rt.BlendOp = D3D12_BLEND_OP_ADD;
      rt.SrcBlendAlpha = D3D12_BLEND_ONE;
      rt.DestBlendAlpha = D3D12_BLEND_ZERO;
      rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
      rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
      return *this;
    }

    PsoBuilder& AdditiveBlend()
    {
      auto& rt = Desc.BlendState.RenderTarget[0];
      rt.BlendEnable = TRUE;
      rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
      rt.DestBlend = D3D12_BLEND_ONE;
      rt.BlendOp = D3D12_BLEND_OP_ADD;
      rt.SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
      rt.DestBlendAlpha = D3D12_BLEND_ONE;
      rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
      rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
      return *this;
    }

    PsoBuilder& PureAdditiveBlend()
    {
      auto& rt = Desc.BlendState.RenderTarget[0];
      rt.BlendEnable = TRUE;
      rt.SrcBlend = D3D12_BLEND_ONE;
      rt.DestBlend = D3D12_BLEND_ONE;
      rt.BlendOp = D3D12_BLEND_OP_ADD;
      rt.SrcBlendAlpha = D3D12_BLEND_ONE;
      rt.DestBlendAlpha = D3D12_BLEND_ONE;
      rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
      rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
      return *this;
    }

    // Reverse-Z depth: GREATER_EQUAL comparison, depth write enabled
    PsoBuilder& DepthReverseZ()
    {
      Desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
      Desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
      Desc.DSVFormat = Core::GetDepthBufferFormat();
      return *this;
    }

    // Reverse-Z depth test but no depth write (transparent/background objects)
    PsoBuilder& DepthReadOnly()
    {
      DepthReverseZ();
      Desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      return *this;
    }

    PsoBuilder& NoDepth()
    {
      Desc.DepthStencilState.DepthEnable = FALSE;
      return *this;
    }

    PsoBuilder& Topology(D3D12_PRIMITIVE_TOPOLOGY_TYPE type) { Desc.PrimitiveTopologyType = type; return *this; }

    [[nodiscard]] com_ptr<ID3D12PipelineState> Build() const
    {
      return PipelineHelpers::CreateGraphicsPSO(Desc);
    }
  };
}
