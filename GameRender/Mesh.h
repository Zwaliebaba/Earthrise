#pragma once

#include "CmoLoader.h"

namespace Neuron::Graphics
{
  // GPU-resident mesh: vertex buffer, index buffer, submesh table.
  class Mesh
  {
  public:
    Mesh() = default;
    ~Mesh() = default;

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) = default;
    Mesh& operator=(Mesh&&) = default;

    // Create GPU resources from parsed CMO data.
    void CreateFromCmoData(const CmoMeshData& data);

    [[nodiscard]] D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const noexcept
    {
      D3D12_VERTEX_BUFFER_VIEW view{};
      if (m_vertexBuffer)
      {
        view.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        view.SizeInBytes = m_vertexCount * m_vertexStride;
        view.StrideInBytes = m_vertexStride;
      }
      return view;
    }

    [[nodiscard]] D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const noexcept
    {
      D3D12_INDEX_BUFFER_VIEW view{};
      if (m_indexBuffer)
      {
        view.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        view.SizeInBytes = m_indexCount * sizeof(uint16_t);
        view.Format = DXGI_FORMAT_R16_UINT;
      }
      return view;
    }

    [[nodiscard]] std::span<const CmoSubmesh> GetSubmeshes() const noexcept { return m_submeshes; }
    [[nodiscard]] uint32_t GetVertexCount() const noexcept { return m_vertexCount; }
    [[nodiscard]] uint32_t GetIndexCount() const noexcept { return m_indexCount; }
    [[nodiscard]] bool IsValid() const noexcept { return m_vertexBuffer != nullptr; }

  private:
    com_ptr<ID3D12Resource> m_vertexBuffer;
    com_ptr<ID3D12Resource> m_indexBuffer;
    uint32_t m_vertexCount = 0;
    uint32_t m_indexCount = 0;
    uint32_t m_vertexStride = sizeof(FlatColorVertex);
    std::vector<CmoSubmesh> m_submeshes;
  };
}
