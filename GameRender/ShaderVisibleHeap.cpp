#include "pch.h"
#include "ShaderVisibleHeap.h"

using namespace Neuron::Graphics;

void ShaderVisibleHeap::Create(uint32_t descriptorsPerFrame, UINT frameCount)
{
  ASSERT(descriptorsPerFrame > 0 && frameCount > 0);

  m_descriptorsPerFrame = descriptorsPerFrame;
  m_frameCount = frameCount;
  m_persistentCount = 0;

  // Reserve extra space for persistent descriptors at the front of the heap.
  constexpr uint32_t PERSISTENT_RESERVE = 64;
  m_totalDescriptors = PERSISTENT_RESERVE + descriptorsPerFrame * frameCount;

  D3D12_DESCRIPTOR_HEAP_DESC desc{};
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  desc.NumDescriptors = m_totalDescriptors;
  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  desc.NodeMask = 1;

  check_hresult(Core::GetD3DDevice()->CreateDescriptorHeap(&desc, IID_GRAPHICS_PPV_ARGS(m_heap)));
  SetName(m_heap.get(), L"ShaderVisibleHeap");

  m_descriptorSize = Core::GetD3DDevice()->GetDescriptorHandleIncrementSize(
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();
  m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();
}

void ShaderVisibleHeap::Destroy()
{
  m_heap = nullptr;
}

void ShaderVisibleHeap::BeginFrame(UINT frameIndex)
{
  DEBUG_ASSERT(frameIndex < m_frameCount);
  // Per-frame rings start after the persistent region.
  constexpr uint32_t PERSISTENT_RESERVE = 64;
  m_frameBase = PERSISTENT_RESERVE + frameIndex * m_descriptorsPerFrame;
  m_currentIndex = 0;
}

DescriptorHandle ShaderVisibleHeap::AllocatePersistent(uint32_t count)
{
  ASSERT(count > 0);
  constexpr uint32_t PERSISTENT_RESERVE = 64;

  if (m_persistentCount + count > PERSISTENT_RESERVE)
  {
    Neuron::Fatal("ShaderVisibleHeap: persistent region exhausted ({} + {} > {})",
      m_persistentCount, count, PERSISTENT_RESERVE);
  }

  const uint32_t absolute = m_persistentCount;
  m_persistentCount += count;

  D3D12_CPU_DESCRIPTOR_HANDLE cpu = m_cpuStart;
  cpu.ptr += static_cast<SIZE_T>(absolute) * m_descriptorSize;

  D3D12_GPU_DESCRIPTOR_HANDLE gpu = m_gpuStart;
  gpu.ptr += static_cast<UINT64>(absolute) * m_descriptorSize;

  return DescriptorHandle(cpu, gpu);
}

DescriptorHandle ShaderVisibleHeap::Allocate(uint32_t count)
{
  ASSERT(count > 0);

  if (m_currentIndex + count > m_descriptorsPerFrame)
  {
    Neuron::DebugTrace("ShaderVisibleHeap: frame region exhausted ({} + {} > {}), skipping draw\n",
      m_currentIndex, count, m_descriptorsPerFrame);
    return {};
  }

  const uint32_t absolute = m_frameBase + m_currentIndex;
  m_currentIndex += count;

#if defined(_DEBUG)
  if (m_currentIndex > m_highWatermark)
  {
    m_highWatermark = m_currentIndex;
    Neuron::DebugTrace("ShaderVisibleHeap: new high watermark = {} / {}\n",
      m_highWatermark, m_descriptorsPerFrame);
  }
#endif

  D3D12_CPU_DESCRIPTOR_HANDLE cpu = m_cpuStart;
  cpu.ptr += static_cast<SIZE_T>(absolute) * m_descriptorSize;

  D3D12_GPU_DESCRIPTOR_HANDLE gpu = m_gpuStart;
  gpu.ptr += static_cast<UINT64>(absolute) * m_descriptorSize;

  return DescriptorHandle(cpu, gpu);
}
