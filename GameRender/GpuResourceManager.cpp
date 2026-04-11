#include "pch.h"
#include "GpuResourceManager.h"

using namespace Neuron::Graphics;

void GpuResourceManager::EnsureCopyInfrastructure()
{
  if (s_copyAllocator)
    return;

  auto device = Core::GetD3DDevice();

  check_hresult(device->CreateCommandAllocator(
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    IID_GRAPHICS_PPV_ARGS(s_copyAllocator)));
  SetName(s_copyAllocator.get(), L"GpuResourceManager CopyAllocator");

  check_hresult(device->CreateCommandList(
    0, D3D12_COMMAND_LIST_TYPE_DIRECT,
    s_copyAllocator.get(), nullptr,
    IID_GRAPHICS_PPV_ARGS(s_copyCommandList)));
  SetName(s_copyCommandList.get(), L"GpuResourceManager CopyCmdList");

  // Close immediately — we'll reset before each use
  check_hresult(s_copyCommandList->Close());

  check_hresult(device->CreateFence(
    0, D3D12_FENCE_FLAG_NONE,
    IID_GRAPHICS_PPV_ARGS(s_copyFence)));
  SetName(s_copyFence.get(), L"GpuResourceManager CopyFence");

  s_copyFenceEvent.attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
  check_bool(static_cast<bool>(s_copyFenceEvent));
}

com_ptr<ID3D12Resource> GpuResourceManager::CreateStaticBuffer(
  const void* data, size_t sizeInBytes, const wchar_t* debugName)
{
  ASSERT(data && sizeInBytes > 0);
  EnsureCopyInfrastructure();

  auto device = Core::GetD3DDevice();

  // 1. Create the default-heap destination resource
  auto defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeInBytes);

  com_ptr<ID3D12Resource> defaultBuffer;
  check_hresult(device->CreateCommittedResource(
    &defaultHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
    D3D12_RESOURCE_STATE_COMMON, nullptr,
    IID_GRAPHICS_PPV_ARGS(defaultBuffer)));

  if (debugName)
    SetName(defaultBuffer.get(), debugName);

  // 2. Create the upload-heap staging resource
  auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

  com_ptr<ID3D12Resource> uploadBuffer;
  check_hresult(device->CreateCommittedResource(
    &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
    IID_GRAPHICS_PPV_ARGS(uploadBuffer)));

  // 3. Copy data to the upload buffer
  void* mapped = nullptr;
  check_hresult(uploadBuffer->Map(0, nullptr, &mapped));
  memcpy(mapped, data, sizeInBytes);
  uploadBuffer->Unmap(0, nullptr);

  // 4. Record and execute the copy command
  check_hresult(s_copyAllocator->Reset());
  check_hresult(s_copyCommandList->Reset(s_copyAllocator.get(), nullptr));

  s_copyCommandList->CopyBufferRegion(defaultBuffer.get(), 0, uploadBuffer.get(), 0, sizeInBytes);

  // Transition to shader-readable state
  auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
    defaultBuffer.get(),
    D3D12_RESOURCE_STATE_COPY_DEST,
    D3D12_RESOURCE_STATE_GENERIC_READ);
  s_copyCommandList->ResourceBarrier(1, &barrier);

  check_hresult(s_copyCommandList->Close());

  ID3D12CommandList* lists[] = { s_copyCommandList.get() };
  Core::GetCommandQueue()->ExecuteCommandLists(1, lists);

  // 5. Wait for the copy to complete
  ++s_copyFenceValue;
  check_hresult(Core::GetCommandQueue()->Signal(s_copyFence.get(), s_copyFenceValue));
  if (s_copyFence->GetCompletedValue() < s_copyFenceValue)
  {
    check_hresult(s_copyFence->SetEventOnCompletion(s_copyFenceValue, s_copyFenceEvent.get()));
    WaitForSingleObjectEx(s_copyFenceEvent.get(), INFINITE, FALSE);
  }

  // uploadBuffer released automatically when com_ptr goes out of scope
  return defaultBuffer;
}
