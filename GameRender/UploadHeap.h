#pragma once

namespace Neuron::Graphics
{
  // CPU-writable, GPU-readable allocation from the upload heap.
  struct UploadAllocation
  {
    void* CpuAddress = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS GpuAddress = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
  };

  // Ring-buffer backed GPU upload heap for per-frame dynamic data.
  // Divided into N regions (one per back buffer) so in-flight frames never overlap.
  class UploadHeap
  {
  public:
    void Create(size_t perFrameSize, UINT frameCount);
    void Destroy();

    void BeginFrame(UINT frameIndex);

    [[nodiscard]] UploadAllocation Allocate(size_t sizeInBytes, size_t alignment = 256);

    [[nodiscard]] ID3D12Resource* GetResource() const noexcept { return m_resource.get(); }
    [[nodiscard]] size_t GetPerFrameSize() const noexcept { return m_perFrameSize; }
    [[nodiscard]] size_t GetCurrentOffset() const noexcept { return m_currentOffset; }

  private:
    com_ptr<ID3D12Resource> m_resource;
    uint8_t* m_cpuBase = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS m_gpuBase = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
    size_t m_totalSize = 0;
    size_t m_perFrameSize = 0;
    size_t m_frameBase = 0;     // Start offset for the current frame's region
    size_t m_currentOffset = 0; // Current allocation position within the frame region
    UINT m_frameCount = 0;
  };
}
