#pragma once

#include "UploadHeap.h"

namespace Neuron::Graphics
{
  // Allocates 256-byte aligned constant buffer blocks from the upload heap.
  // Returns GPU virtual address suitable for root CBV binding.
  class ConstantBufferAllocator
  {
  public:
    void Initialize(UploadHeap* uploadHeap) noexcept { m_uploadHeap = uploadHeap; }

    // Allocate a CB block. Size is rounded up to 256-byte alignment (D3D12 requirement).
    [[nodiscard]] UploadAllocation Allocate(size_t sizeInBytes)
    {
      ASSERT(m_uploadHeap);
      return m_uploadHeap->Allocate(sizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    }

    template <typename T>
    [[nodiscard]] UploadAllocation Allocate()
    {
      return Allocate(sizeof(T));
    }

  private:
    UploadHeap* m_uploadHeap = nullptr;
  };
}
