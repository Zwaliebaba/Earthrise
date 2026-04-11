#include "pch.h"
#include "Mesh.h"
#include "GpuResourceManager.h"

using namespace Neuron::Graphics;

void Mesh::CreateFromCmoData(const CmoMeshData& data)
{
  ASSERT(!data.Vertices.empty() && !data.Indices.empty());

  m_vertexCount = static_cast<uint32_t>(data.Vertices.size());
  m_indexCount = static_cast<uint32_t>(data.Indices.size());
  m_vertexStride = sizeof(FlatColorVertex);
  m_submeshes = data.Submeshes;

  m_vertexBuffer = GpuResourceManager::CreateStaticBuffer(
    data.Vertices.data(),
    data.Vertices.size() * sizeof(FlatColorVertex),
    L"MeshVB");

  m_indexBuffer = GpuResourceManager::CreateStaticBuffer(
    data.Indices.data(),
    data.Indices.size() * sizeof(uint16_t),
    L"MeshIB");
}
