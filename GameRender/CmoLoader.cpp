#include "pch.h"
#include "CmoLoader.h"

using namespace Neuron::Graphics;

namespace
{
  // VSD3DStarter vertex layout: Position(12) + Normal(12) + Tangent(16) + Color(4) + TexCoord(8) = 52 bytes
  static constexpr size_t CMO_VERTEX_STRIDE = 52;

  struct CmoMaterialSettings
  {
    XMFLOAT4 Ambient;
    XMFLOAT4 Diffuse;
    XMFLOAT4 Specular;
    float SpecularPower;
    XMFLOAT4 Emissive;
    XMFLOAT4X4 UVTransform;
  };

  struct CmoSubmeshRaw
  {
    uint32_t MaterialIndex;
    uint32_t IndexBufferIndex;
    uint32_t VertexBufferIndex;
    uint32_t StartIndex;
    uint32_t PrimCount;
  };

  class BinaryReader
  {
  public:
    BinaryReader(const uint8_t* data, size_t size) noexcept
      : m_data(data), m_size(size), m_pos(0) {}

    template <typename T>
    T Read()
    {
      CheckBounds(sizeof(T));
      T value;
      memcpy(&value, m_data + m_pos, sizeof(T));
      m_pos += sizeof(T);
      return value;
    }

    void ReadBytes(void* dest, size_t count)
    {
      CheckBounds(count);
      memcpy(dest, m_data + m_pos, count);
      m_pos += count;
    }

    void Skip(size_t bytes)
    {
      CheckBounds(bytes);
      m_pos += bytes;
    }

    std::wstring ReadString()
    {
      const auto len = Read<uint32_t>(); // length in wchar_t (including null)
      if (len == 0)
        return {};
      CheckBounds(len * sizeof(wchar_t));
      std::wstring result(reinterpret_cast<const wchar_t*>(m_data + m_pos), len - 1); // exclude null
      m_pos += len * sizeof(wchar_t);
      return result;
    }

    [[nodiscard]] bool HasData() const noexcept { return m_pos < m_size; }
    [[nodiscard]] size_t Position() const noexcept { return m_pos; }
    [[nodiscard]] size_t Remaining() const noexcept { return m_size - m_pos; }

  private:
    void CheckBounds(size_t count) const
    {
      if (m_pos + count > m_size)
        Neuron::Fatal("CmoLoader: read past end of data (pos={}, count={}, size={})", m_pos, count, m_size);
    }

    const uint8_t* m_data;
    size_t m_size;
    size_t m_pos;
  };
}

std::vector<CmoMeshData> CmoLoader::LoadFromMemory(const uint8_t* data, size_t size)
{
  BinaryReader reader(data, size);
  std::vector<CmoMeshData> meshes;

  // CMO files begin with a mesh count
  const auto numMeshes = reader.Read<uint32_t>();

  for (uint32_t i = 0; i < numMeshes && reader.HasData(); ++i)
  {
    CmoMeshData mesh;

    // Mesh name
    mesh.Name = reader.ReadString();

    // Materials
    const auto numMaterials = reader.Read<uint32_t>();
    for (uint32_t m = 0; m < numMaterials; ++m)
    {
      // Material name
      reader.ReadString();

      // Material settings
      reader.Skip(sizeof(CmoMaterialSettings));

      // Pixel shader name
      reader.ReadString();

      // 8 texture path strings
      for (int t = 0; t < 8; ++t)
        reader.ReadString();
    }

    // Skeleton
    const auto hasSkeleton = reader.Read<uint8_t>();
    if (hasSkeleton)
    {
      const auto numBones = reader.Read<uint32_t>();
      for (uint32_t b = 0; b < numBones; ++b)
      {
        reader.ReadString();          // bone name
        reader.Skip(sizeof(int32_t)); // parent index
        reader.Skip(sizeof(XMFLOAT4X4) * 3); // invBindPose, bindPose, localTransform
      }
    }

    // Submeshes
    const auto numSubmeshes = reader.Read<uint32_t>();
    std::vector<CmoSubmeshRaw> rawSubmeshes(numSubmeshes);
    for (uint32_t s = 0; s < numSubmeshes; ++s)
    {
      rawSubmeshes[s].MaterialIndex = reader.Read<uint32_t>();
      rawSubmeshes[s].IndexBufferIndex = reader.Read<uint32_t>();
      rawSubmeshes[s].VertexBufferIndex = reader.Read<uint32_t>();
      rawSubmeshes[s].StartIndex = reader.Read<uint32_t>();
      rawSubmeshes[s].PrimCount = reader.Read<uint32_t>();
    }

    // Index buffers
    const auto numIBs = reader.Read<uint32_t>();
    std::vector<std::vector<uint16_t>> indexBuffers(numIBs);
    for (uint32_t ib = 0; ib < numIBs; ++ib)
    {
      const auto numIndices = reader.Read<uint32_t>();
      indexBuffers[ib].resize(numIndices);
      reader.ReadBytes(indexBuffers[ib].data(), numIndices * sizeof(uint16_t));
    }

    // Vertex buffers
    const auto numVBs = reader.Read<uint32_t>();
    std::vector<std::vector<FlatColorVertex>> vertexBuffers(numVBs);
    std::vector<std::vector<XMFLOAT2>> texCoordBuffers(numVBs);
    for (uint32_t vb = 0; vb < numVBs; ++vb)
    {
      const auto numVertices = reader.Read<uint32_t>();
      vertexBuffers[vb].resize(numVertices);
      texCoordBuffers[vb].resize(numVertices);

      for (uint32_t v = 0; v < numVertices; ++v)
      {
        // Position(12) + Normal(12) + Tangent(16) + Color(4) + TexCoord(8) = 52
        reader.ReadBytes(&vertexBuffers[vb][v].Position, sizeof(XMFLOAT3));
        reader.ReadBytes(&vertexBuffers[vb][v].Normal, sizeof(XMFLOAT3));
        reader.Skip(sizeof(XMFLOAT4) + sizeof(uint32_t)); // Skip tangent(16) + color(4)
        reader.ReadBytes(&texCoordBuffers[vb][v], sizeof(XMFLOAT2));
      }
    }

    // Skinning vertex buffers (skip)
    if (hasSkeleton)
    {
      const auto numSkinVBs = reader.Read<uint32_t>();
      for (uint32_t svb = 0; svb < numSkinVBs; ++svb)
      {
        const auto numSkinVerts = reader.Read<uint32_t>();
        reader.Skip(numSkinVerts * (4 * sizeof(uint32_t) + 4 * sizeof(float))); // boneIndex[4] + boneWeight[4]
      }
    }

    // Merge all VBs/IBs into a single vertex+index list + build submeshes
    // Most CMO files have 1 VB and 1 IB, but handle multiple for correctness.
    uint32_t vertexOffset = 0;
    uint32_t indexOffset = 0;
    std::vector<uint32_t> vbBaseVertex(numVBs);
    std::vector<uint32_t> ibBaseIndex(numIBs);

    for (uint32_t vb = 0; vb < numVBs; ++vb)
    {
      vbBaseVertex[vb] = vertexOffset;
      mesh.Vertices.insert(mesh.Vertices.end(), vertexBuffers[vb].begin(), vertexBuffers[vb].end());
      mesh.TexCoords.insert(mesh.TexCoords.end(), texCoordBuffers[vb].begin(), texCoordBuffers[vb].end());
      vertexOffset += static_cast<uint32_t>(vertexBuffers[vb].size());
    }

    for (uint32_t ib = 0; ib < numIBs; ++ib)
    {
      ibBaseIndex[ib] = indexOffset;
      // Rebase indices by the VB offset they reference
      for (auto idx : indexBuffers[ib])
        mesh.Indices.push_back(idx); // Index rebasing handled per-submesh
      indexOffset += static_cast<uint32_t>(indexBuffers[ib].size());
    }

    // Build submesh descriptors
    for (const auto& raw : rawSubmeshes)
    {
      CmoSubmesh sub{};
      sub.MaterialIndex = raw.MaterialIndex;
      sub.StartIndex = ibBaseIndex[raw.IndexBufferIndex] + raw.StartIndex;
      sub.IndexCount = raw.PrimCount * 3;
      mesh.Submeshes.push_back(sub);
    }

    // Adjust indices for multi-VB meshes: add the base vertex of the VB they reference
    // For single-VB meshes (common case), base is 0 and this is a no-op effectively.
    // We handle multi-VB by adjusting the indices in the merged index buffer.
    if (numVBs > 1)
    {
      for (const auto& raw : rawSubmeshes)
      {
        const uint32_t base = vbBaseVertex[raw.VertexBufferIndex];
        if (base == 0) continue;
        const uint32_t start = ibBaseIndex[raw.IndexBufferIndex] + raw.StartIndex;
        const uint32_t count = raw.PrimCount * 3;
        for (uint32_t i = start; i < start + count && i < mesh.Indices.size(); ++i)
          mesh.Indices[i] += static_cast<uint16_t>(base);
      }
    }

    meshes.push_back(std::move(mesh));
  }

  return meshes;
}

CmoMeshData ProceduralMesh::GenerateUVSphere(uint32_t rings, uint32_t slices)
{
  CmoMeshData data;
  data.Name = L"ProceduralSphere";

  // Generate vertices: (rings + 1) latitude rows × (slices + 1) longitude columns.
  // Unit radius — caller applies scale via the world matrix.
  for (uint32_t r = 0; r <= rings; ++r)
  {
    float theta = static_cast<float>(r) * XM_PI / static_cast<float>(rings);
    float sinT = sinf(theta);
    float cosT = cosf(theta);

    for (uint32_t s = 0; s <= slices; ++s)
    {
      float phi = static_cast<float>(s) * XM_2PI / static_cast<float>(slices);
      float sinP = sinf(phi);
      float cosP = cosf(phi);

      XMFLOAT3 pos = { sinT * cosP, cosT, sinT * sinP };
      XMFLOAT3 normal = pos; // Unit sphere — normal == position

      data.Vertices.push_back({ pos, normal });
      data.TexCoords.push_back({
        static_cast<float>(s) / static_cast<float>(slices),
        static_cast<float>(r) / static_cast<float>(rings)
      });
    }
  }

  // Generate triangle indices.
  uint32_t vertsPerRow = slices + 1;
  for (uint32_t r = 0; r < rings; ++r)
  {
    for (uint32_t s = 0; s < slices; ++s)
    {
      uint16_t tl = static_cast<uint16_t>(r * vertsPerRow + s);
      uint16_t tr = static_cast<uint16_t>(tl + 1);
      uint16_t bl = static_cast<uint16_t>((r + 1) * vertsPerRow + s);
      uint16_t br = static_cast<uint16_t>(bl + 1);

      // Upper triangle
      data.Indices.push_back(tl);
      data.Indices.push_back(bl);
      data.Indices.push_back(tr);

      // Lower triangle
      data.Indices.push_back(tr);
      data.Indices.push_back(bl);
      data.Indices.push_back(br);
    }
  }

  // Single submesh covering all indices
  CmoSubmesh sub{};
  sub.MaterialIndex = 0;
  sub.StartIndex = 0;
  sub.IndexCount = static_cast<uint32_t>(data.Indices.size());
  data.Submeshes.push_back(sub);

  return data;
}

std::vector<CmoMeshData> CmoLoader::LoadFromFile(const std::wstring& filePath)
{
  // Read entire file into memory
  HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (hFile == INVALID_HANDLE_VALUE)
    Neuron::Fatal("CmoLoader: failed to open file");

  LARGE_INTEGER fileSize;
  if (!GetFileSizeEx(hFile, &fileSize))
  {
    CloseHandle(hFile);
    Neuron::Fatal("CmoLoader: failed to get file size");
  }

  std::vector<uint8_t> buffer(static_cast<size_t>(fileSize.QuadPart));
  DWORD bytesRead = 0;
  if (!ReadFile(hFile, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr))
  {
    CloseHandle(hFile);
    Neuron::Fatal("CmoLoader: failed to read file");
  }
  CloseHandle(hFile);

  return LoadFromMemory(buffer.data(), buffer.size());
}
