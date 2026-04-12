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

com_ptr<ID3D12Resource> GpuResourceManager::CreateStaticTexture(
  const CpuImage& image, const wchar_t* debugName)
{
  ASSERT(image.Width > 0 && image.Height > 0 && !image.Pixels.empty());
  EnsureCopyInfrastructure();

  auto device = Core::GetD3DDevice();

  // 1. Create the default-heap texture resource
  D3D12_RESOURCE_DESC texDesc = {};
  texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texDesc.Width = image.Width;
  texDesc.Height = image.Height;
  texDesc.DepthOrArraySize = 1;
  texDesc.MipLevels = 1;
  texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  texDesc.SampleDesc.Count = 1;
  texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  auto defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  com_ptr<ID3D12Resource> texture;
  check_hresult(device->CreateCommittedResource(
    &defaultHeapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
    D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
    IID_GRAPHICS_PPV_ARGS(texture)));

  if (debugName)
    SetName(texture.get(), debugName);

  // 2. Compute upload buffer size and row pitch
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
  UINT64 totalBytes = 0;
  device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, nullptr, nullptr, &totalBytes);

  // 3. Create the upload-heap staging buffer
  auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(totalBytes);

  com_ptr<ID3D12Resource> uploadBuffer;
  check_hresult(device->CreateCommittedResource(
    &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
    IID_GRAPHICS_PPV_ARGS(uploadBuffer)));

  // 4. Copy pixel data into upload buffer (respecting row pitch alignment)
  uint8_t* mapped = nullptr;
  check_hresult(uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mapped)));

  const uint32_t srcRowPitch = image.Width * sizeof(uint32_t);
  const uint8_t* srcData = reinterpret_cast<const uint8_t*>(image.Pixels.data());

  for (uint32_t row = 0; row < image.Height; ++row)
  {
    memcpy(mapped + footprint.Offset + row * footprint.Footprint.RowPitch,
           srcData + row * srcRowPitch, srcRowPitch);
  }
  uploadBuffer->Unmap(0, nullptr);

  // 5. Record and execute the copy command
  check_hresult(s_copyAllocator->Reset());
  check_hresult(s_copyCommandList->Reset(s_copyAllocator.get(), nullptr));

  D3D12_TEXTURE_COPY_LOCATION dst{};
  dst.pResource = texture.get();
  dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dst.SubresourceIndex = 0;

  D3D12_TEXTURE_COPY_LOCATION src{};
  src.pResource = uploadBuffer.get();
  src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  src.PlacedFootprint = footprint;

  s_copyCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

  // Transition to shader-readable state
  auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
    texture.get(),
    D3D12_RESOURCE_STATE_COPY_DEST,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  s_copyCommandList->ResourceBarrier(1, &barrier);

  check_hresult(s_copyCommandList->Close());

  ID3D12CommandList* lists[] = { s_copyCommandList.get() };
  Core::GetCommandQueue()->ExecuteCommandLists(1, lists);

  // 6. Wait for the copy to complete
  ++s_copyFenceValue;
  check_hresult(Core::GetCommandQueue()->Signal(s_copyFence.get(), s_copyFenceValue));
  if (s_copyFence->GetCompletedValue() < s_copyFenceValue)
  {
    check_hresult(s_copyFence->SetEventOnCompletion(s_copyFenceValue, s_copyFenceEvent.get()));
    WaitForSingleObjectEx(s_copyFenceEvent.get(), INFINITE, FALSE);
  }

  return texture;
}
