#pragma once

namespace Neuron::Graphics
{
  // Utility functions to build root signatures and PSOs.
  namespace PipelineHelpers
  {
    // Compile HLSL source at runtime. Returns shader bytecode blob.
    [[nodiscard]] com_ptr<ID3DBlob> CompileShader(
      const char* source, size_t sourceSize,
      const char* entryPoint, const char* target,
      const char* debugName = nullptr);

    // Create a root signature from a serialized blob.
    [[nodiscard]] com_ptr<ID3D12RootSignature> CreateRootSignature(
      const D3D12_ROOT_SIGNATURE_DESC& desc);

    // Create a graphics PSO.
    [[nodiscard]] com_ptr<ID3D12PipelineState> CreateGraphicsPSO(
      const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);
  }
}
