#include "pch.h"
#include "PipelineHelpers.h"

using namespace Neuron::Graphics;

com_ptr<ID3D12RootSignature> PipelineHelpers::CreateRootSignature(
  const D3D12_ROOT_SIGNATURE_DESC& desc)
{
  com_ptr<ID3DBlob> serialized;
  com_ptr<ID3DBlob> errors;

  // Wrap the 1.0 desc in a versioned container.  Keep Version as 1_0 so
  // D3D12SerializeVersionedRootSignature performs the lossless 1.0 → 1.1
  // promotion internally.  Setting Version to 1_1 here would be wrong
  // because the union's Desc_1_1 (D3D12_ROOT_PARAMETER1) has a Flags
  // field that doesn't exist in the 1.0 layout, leading to reads of
  // uninitialized memory.
  D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedDesc = {};
  versionedDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
  versionedDesc.Desc_1_0 = desc;

  HRESULT hr = D3D12SerializeVersionedRootSignature(&versionedDesc,
    serialized.put(), errors.put());

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
