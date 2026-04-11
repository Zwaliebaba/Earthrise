#include "pch.h"
#include "UploadHeap.h"

using namespace Neuron::Graphics;

void UploadHeap::Create(size_t perFrameSize, UINT frameCount)
{
  ASSERT(perFrameSize > 0 && frameCount > 0);

  m_perFrameSize = perFrameSize;
  m_frameCount = frameCount;
  m_totalSize = perFrameSize * frameCount;

  auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_totalSize);

  check_hresult(Core::GetD3DDevice()->CreateCommittedResource(
    &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
    IID_GRAPHICS_PPV_ARGS(m_resource)));

  SetName(m_resource.get(), L"UploadHeap");

  // Map the entire buffer persistently (upload heaps stay mapped)
  check_hresult(m_resource->Map(0, nullptr, reinterpret_cast<void**>(&m_cpuBase)));
  m_gpuBase = m_resource->GetGPUVirtualAddress();
}

void UploadHeap::Destroy()
{
  if (m_resource)
  {
    m_resource->Unmap(0, nullptr);
    m_cpuBase = nullptr;
  }
  m_resource = nullptr;
  m_gpuBase = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
  m_totalSize = 0;
}

void UploadHeap::BeginFrame(UINT frameIndex)
{
  DEBUG_ASSERT(frameIndex < m_frameCount);
  m_frameBase = static_cast<size_t>(frameIndex) * m_perFrameSize;
  m_currentOffset = 0;
}

UploadAllocation UploadHeap::Allocate(size_t sizeInBytes, size_t alignment)
{
  ASSERT(sizeInBytes > 0);

  // Align the current offset
  const size_t aligned = (m_currentOffset + alignment - 1) & ~(alignment - 1);

  if (aligned + sizeInBytes > m_perFrameSize)
    Neuron::Fatal("UploadHeap: frame region exhausted ({} + {} > {})", aligned, sizeInBytes, m_perFrameSize);

  const size_t absoluteOffset = m_frameBase + aligned;
  m_currentOffset = aligned + sizeInBytes;

  return { m_cpuBase + absoluteOffset, m_gpuBase + absoluteOffset };
}
