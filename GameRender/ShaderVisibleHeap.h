#pragma once

namespace Neuron::Graphics
{
  // Per-frame ring allocator for shader-visible CBV/SRV/UAV descriptors.
  // Used for SRV bindings in post-processing passes.
  // Bind the heap once per frame via SetDescriptorHeaps.
  class ShaderVisibleHeap
  {
  public:
    void Create(uint32_t descriptorsPerFrame, UINT frameCount);
    void Destroy();

    void BeginFrame(UINT frameIndex);

    // Allocate descriptors from the current frame's region.
    [[nodiscard]] DescriptorHandle Allocate(uint32_t count = 1);

    [[nodiscard]] ID3D12DescriptorHeap* GetHeap() const noexcept { return m_heap.get(); }
    [[nodiscard]] uint32_t GetDescriptorSize() const noexcept { return m_descriptorSize; }

  private:
    com_ptr<ID3D12DescriptorHeap> m_heap;
    uint32_t m_descriptorSize = 0;
    uint32_t m_totalDescriptors = 0;
    uint32_t m_descriptorsPerFrame = 0;
    uint32_t m_frameBase = 0;       // First descriptor index for current frame
    uint32_t m_currentIndex = 0;    // Next free index within the frame's region
    UINT m_frameCount = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart{};
  };
}
