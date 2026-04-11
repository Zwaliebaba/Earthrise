#include "pch.h"
#include "PipelineHelpers.h"

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

using namespace Neuron::Graphics;

com_ptr<ID3DBlob> PipelineHelpers::CompileShader(
  const char* source, size_t sourceSize,
  const char* entryPoint, const char* target,
  const char* debugName)
{
  UINT compileFlags = 0;
#if defined(_DEBUG)
  compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
  compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

  com_ptr<ID3DBlob> byteCode;
  com_ptr<ID3DBlob> errors;

  HRESULT hr = D3DCompile(source, sourceSize, debugName, nullptr, nullptr,
    entryPoint, target, compileFlags, 0, byteCode.put(), errors.put());

  if (FAILED(hr))
  {
    if (errors)
    {
      Neuron::Fatal("Shader compilation failed: {}",
        static_cast<const char*>(errors->GetBufferPointer()));
    }
    check_hresult(hr);
  }

  return byteCode;
}

com_ptr<ID3D12RootSignature> PipelineHelpers::CreateRootSignature(
  const D3D12_ROOT_SIGNATURE_DESC& desc)
{
  com_ptr<ID3DBlob> serialized;
  com_ptr<ID3DBlob> errors;

  HRESULT hr = D3D12SerializeRootSignature(&desc,
    D3D_ROOT_SIGNATURE_VERSION_1, serialized.put(), errors.put());

  if (FAILED(hr))
  {
    if (errors)
    {
      Neuron::Fatal("Root signature serialization failed: {}",
        static_cast<const char*>(errors->GetBufferPointer()));
    }
    check_hresult(hr);
  }

  com_ptr<ID3D12RootSignature> rootSig;
  check_hresult(Core::GetD3DDevice()->CreateRootSignature(
    0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
    IID_GRAPHICS_PPV_ARGS(rootSig)));

  return rootSig;
}

com_ptr<ID3D12PipelineState> PipelineHelpers::CreateGraphicsPSO(
  const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
{
  com_ptr<ID3D12PipelineState> pso;
  check_hresult(Core::GetD3DDevice()->CreateGraphicsPipelineState(
    &desc, IID_GRAPHICS_PPV_ARGS(pso)));
  return pso;
}
