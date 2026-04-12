#pragma once

#include "DdsLoader.h"

namespace Neuron::Graphics
{
  // Creates GPU-optimal (default-heap) buffers and textures from CPU data.
  // Internally stages through an upload heap and executes a GPU copy.
  class GpuResourceManager
  {
  public:
    // Create a vertex/index buffer in default heap.
    // Blocks until the GPU copy completes.
    [[nodiscard]] static com_ptr<ID3D12Resource> CreateStaticBuffer(
      const void* data, size_t sizeInBytes, const wchar_t* debugName = nullptr);

    // Create a 2D texture in default heap from CPU-side RGBA8888 pixel data.
    // Returns the texture resource in PIXEL_SHADER_RESOURCE state.
    [[nodiscard]] static com_ptr<ID3D12Resource> CreateStaticTexture(
      const CpuImage& image, const wchar_t* debugName = nullptr);

  private:
    // Internal copy command infrastructure (created lazily)
    static void EnsureCopyInfrastructure();

    inline static com_ptr<ID3D12CommandAllocator> s_copyAllocator;
    inline static com_ptr<ID3D12GraphicsCommandList> s_copyCommandList;
    inline static com_ptr<ID3D12Fence> s_copyFence;
    inline static UINT64 s_copyFenceValue = 0;
    inline static handle s_copyFenceEvent;
  };
}
