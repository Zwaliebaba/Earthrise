#pragma once

namespace Neuron::Graphics
{
  // Shader-visible CBV/SRV/UAV descriptor heap with two regions:
  //  1. Persistent segment: first N slots, set once (init/resize), never reset.
  //  2. Per-frame ring: remaining slots, reset each frame via BeginFrame.
  // Bind the heap once per frame via SetDescriptorHeaps.
  class ShaderVisibleHeap
  {
  public:
    void Create(uint32_t descriptorsPerFrame, UINT frameCount);
    void Destroy();

    void BeginFrame(UINT frameIndex);

    // Allocate from the persistent segment (never reset). Call during init/resize.
    [[nodiscard]] DescriptorHandle AllocatePersistent(uint32_t count = 1);

    // Allocate from the current frame's per-frame ring region.
    [[nodiscard]] DescriptorHandle Allocate(uint32_t count = 1);

    [[nodiscard]] ID3D12DescriptorHeap* GetHeap() const noexcept { return m_heap.get(); }
    [[nodiscard]] uint32_t GetDescriptorSize() const noexcept { return m_descriptorSize; }

  private:
    com_ptr<ID3D12DescriptorHeap> m_heap;
    uint32_t m_descriptorSize = 0;
    uint32_t m_totalDescriptors = 0;
    uint32_t m_descriptorsPerFrame = 0;
    uint32_t m_persistentCount = 0;   // Number of persistent descriptors allocated
    uint32_t m_frameBase = 0;         // First descriptor index for current frame
    uint32_t m_currentIndex = 0;      // Next free index within the frame's region
    UINT m_frameCount = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart{};

#if defined(_DEBUG)
    uint32_t m_highWatermark = 0;     // Peak descriptors used in a single frame
#endif
  };
}
