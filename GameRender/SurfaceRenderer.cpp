#include "pch.h"
#include "SurfaceRenderer.h"

using namespace Neuron::Graphics;

namespace
{
  // Landscape texture files under GameData/Terrain/
  constexpr const wchar_t* c_terrainFolder = L"Terrain\\";
}

void SurfaceRenderer::Initialize()
{
  m_pipeline.Initialize();

  // Load all landscape textures
  for (auto type : RangeSurfaceType())
  {
    std::wstring path = FileSys::GetHomeDirectory();
    path += c_terrainFolder;
    path += LandscapeTexture::GetTextureFilename(type);

    m_textures[static_cast<size_t>(type)] = LandscapeTexture::LoadFromFile(path);

    if (!m_textures[static_cast<size_t>(type)])
      Neuron::DebugTrace("SurfaceRenderer: failed to load landscape texture for type {}",
                         static_cast<int>(type));
  }
}

void SurfaceRenderer::BeginFrame(ID3D12GraphicsCommandList* cmdList,
  ConstantBufferAllocator& cbAlloc,
  const Camera& camera)
{
  m_pipeline.BindPipeline(cmdList);

  FrameConstants fc{};
  XMStoreFloat4x4(&fc.ViewProjection, XMMatrixTranspose(camera.GetViewProjectionMatrix()));
  XMStoreFloat3(&fc.CameraPosition, camera.GetPosition());
  XMStoreFloat3(&fc.LightDirection, camera.GetLightDirection());
  fc.AmbientIntensity = 0.3f;

  m_pipeline.SetFrameConstants(cmdList, cbAlloc, fc);
}

SurfaceMesh* SurfaceRenderer::GetSurfaceMesh(std::string_view meshKey,
                                             Neuron::SurfaceType surfaceType)
{
  uint64_t key = ComputeCacheKey(meshKey, surfaceType);

  auto it = m_meshCache.find(key);
  if (it != m_meshCache.end())
    return it->second.get();

  // Load CMO from disk (same path resolution as MeshCache)
  std::wstring filePath = FileSys::GetHomeDirectory();
  filePath += L"Meshes\\";
  for (char c : meshKey)
    filePath += static_cast<wchar_t>(c == '/' ? '\\' : c);
  filePath += L".cmo";

  if (GetFileAttributesW(filePath.c_str()) == INVALID_FILE_ATTRIBUTES)
  {
    // No .cmo on disk — generate a procedural sphere (planet fallback)
    Neuron::DebugTrace("SurfaceRenderer: mesh file not found, generating procedural sphere — {}",
                       std::string(meshKey));
    auto sphereData = ProceduralMesh::GenerateUVSphere(24, 24);
    auto surfMesh = BuildSurfaceMesh(sphereData, surfaceType);
    auto* ptr = surfMesh.get();
    m_meshCache[key] = std::move(surfMesh);
    return ptr;
  }

  auto meshDataVec = CmoLoader::LoadFromFile(filePath);
  if (meshDataVec.empty())
    return nullptr;

  auto surfMesh = BuildSurfaceMesh(meshDataVec[0], surfaceType);
  auto* ptr = surfMesh.get();
  m_meshCache[key] = std::move(surfMesh);
  return ptr;
}

void XM_CALLCONV SurfaceRenderer::RenderObject(ID3D12GraphicsCommandList* cmdList,
  ConstantBufferAllocator& cbAlloc,
  const SurfaceMesh* mesh, FXMMATRIX world)
{
  if (!mesh || !mesh->VertexBuffer)
    return;

  SurfaceDrawConstants dc{};
  XMStoreFloat4x4(&dc.World, XMMatrixTranspose(world));
  m_pipeline.SetDrawConstants(cmdList, cbAlloc, dc);

  auto vbView = mesh->GetVertexBufferView();
  auto ibView = mesh->GetIndexBufferView();
  cmdList->IASetVertexBuffers(0, 1, &vbView);
  cmdList->IASetIndexBuffer(&ibView);

  for (const auto& submesh : mesh->Submeshes)
  {
    cmdList->DrawIndexedInstanced(submesh.IndexCount, 1, submesh.StartIndex, 0, 0);
  }
}

std::unique_ptr<SurfaceMesh> SurfaceRenderer::BuildSurfaceMesh(
  const CmoMeshData& meshData, Neuron::SurfaceType surfaceType)
{
  const LandscapeTexture* texture = m_textures[static_cast<size_t>(surfaceType)].get();
  if (!texture)
  {
    // Fallback to Default if the requested type is missing
    texture = m_textures[static_cast<size_t>(Neuron::SurfaceType::Default)].get();
    if (!texture)
      return nullptr;
  }

  // Compute m_highest: maximum Y coordinate in the mesh
  float highest = 0.001f; // avoid division by zero
  for (const auto& v : meshData.Vertices)
  {
    float absY = fabsf(v.Position.y);
    if (absY > highest)
      highest = absY;
  }

  // Build SurfaceVertex array (Position + Normal + Color).
  // TexCoord0 will hold barycentric coordinates after unindexing.
  std::vector<SurfaceVertex> surfVerts(meshData.Vertices.size());
  for (size_t i = 0; i < meshData.Vertices.size(); ++i)
  {
    surfVerts[i].Position  = meshData.Vertices[i].Position;
    surfVerts[i].Normal    = meshData.Vertices[i].Normal;
    surfVerts[i].Color     = { 1.0f, 1.0f, 1.0f, 1.0f };
    surfVerts[i].TexCoord0 = { 0.0f, 0.0f };
  }

  // Assign per-vertex colors from landscape texture
  BuildColorArray(surfVerts, meshData.Indices, texture, highest);

  // Unindex: duplicate vertices per triangle so each vertex gets unique
  // barycentric coordinates for wireframe edge detection in the pixel shader.
  // v0=(1,0), v1=(0,1), v2=(0,0) → third component derived as 1-x-y.
  const size_t triCount = meshData.Indices.size() / 3;
  std::vector<SurfaceVertex> unindexedVerts;
  unindexedVerts.reserve(triCount * 3);
  std::vector<uint16_t> unindexedIndices;
  unindexedIndices.reserve(triCount * 3);

  static constexpr XMFLOAT2 baryCoords[3] = {
    { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f }
  };

  for (size_t t = 0; t < triCount; ++t)
  {
    uint16_t i0 = meshData.Indices[t * 3 + 0];
    uint16_t i1 = meshData.Indices[t * 3 + 1];
    uint16_t i2 = meshData.Indices[t * 3 + 2];

    auto baseIdx = static_cast<uint16_t>(unindexedVerts.size());

    SurfaceVertex v0 = surfVerts[i0]; v0.TexCoord0 = baryCoords[0];
    SurfaceVertex v1 = surfVerts[i1]; v1.TexCoord0 = baryCoords[1];
    SurfaceVertex v2 = surfVerts[i2]; v2.TexCoord0 = baryCoords[2];

    unindexedVerts.push_back(v0);
    unindexedVerts.push_back(v1);
    unindexedVerts.push_back(v2);

    unindexedIndices.push_back(baseIdx);
    unindexedIndices.push_back(static_cast<uint16_t>(baseIdx + 1));
    unindexedIndices.push_back(static_cast<uint16_t>(baseIdx + 2));
  }

  // Rebuild submeshes for the unindexed layout (indices are now sequential)
  std::vector<CmoSubmesh> unindexedSubmeshes;
  for (const auto& sub : meshData.Submeshes)
  {
    CmoSubmesh newSub{};
    newSub.MaterialIndex = sub.MaterialIndex;
    newSub.StartIndex = sub.StartIndex;
    newSub.IndexCount = sub.IndexCount;
    unindexedSubmeshes.push_back(newSub);
  }

  // Upload to GPU
  auto mesh = std::make_unique<SurfaceMesh>();
  mesh->VertexCount = static_cast<uint32_t>(unindexedVerts.size());
  mesh->IndexCount  = static_cast<uint32_t>(unindexedIndices.size());
  mesh->Submeshes   = std::move(unindexedSubmeshes);

  mesh->VertexBuffer = GpuResourceManager::CreateStaticBuffer(
    unindexedVerts.data(),
    unindexedVerts.size() * sizeof(SurfaceVertex),
    L"SurfaceMeshVB");

  mesh->IndexBuffer = GpuResourceManager::CreateStaticBuffer(
    unindexedIndices.data(),
    unindexedIndices.size() * sizeof(uint16_t),
    L"SurfaceMeshIB");

  return mesh;
}

void SurfaceRenderer::BuildColorArray(std::vector<SurfaceVertex>& verts,
                                      const std::vector<uint16_t>& indices,
                                      const LandscapeTexture* texture,
                                      float highest)
{
  if (verts.empty() || indices.empty())
    return;

  // Accumulate colors per vertex from all triangles it participates in.
  // Each vertex gets the average color of its adjacent triangles.
  struct ColorAccum
  {
    float r = 0, g = 0, b = 0, a = 0;
    uint32_t count = 0;
  };
  std::vector<ColorAccum> accum(verts.size());

  const size_t triCount = indices.size() / 3;
  for (size_t t = 0; t < triCount; ++t)
  {
    uint16_t i0 = indices[t * 3 + 0];
    uint16_t i1 = indices[t * 3 + 1];
    uint16_t i2 = indices[t * 3 + 2];

    if (i0 >= verts.size() || i1 >= verts.size() || i2 >= verts.size())
      continue;

    const XMFLOAT3& v0 = verts[i0].Position;
    const XMFLOAT3& v1 = verts[i1].Position;
    const XMFLOAT3& v2 = verts[i2].Position;

    // Triangle center
    float cx = (v0.x + v1.x + v2.x) * 0.33333f;
    float cy = (v0.y + v1.y + v2.y) * 0.33333f;
    float cz = (v0.z + v1.z + v2.z) * 0.33333f;

    // Use the third vertex's normal for gradient (matching original algorithm)
    float gradient = verts[i2].Normal.y;

    XMFLOAT4 col = GetLandscapeColour(cy, gradient,
      static_cast<unsigned int>(fabsf(cx)),
      static_cast<unsigned int>(fabsf(cz)),
      texture, highest);

    // Accumulate on all 3 vertices of the triangle
    for (uint16_t idx : { i0, i1, i2 })
    {
      accum[idx].r += col.x;
      accum[idx].g += col.y;
      accum[idx].b += col.z;
      accum[idx].a += col.w;
      accum[idx].count++;
    }
  }

  // Average accumulated colors
  for (size_t i = 0; i < verts.size(); ++i)
  {
    if (accum[i].count > 0)
    {
      float inv = 1.0f / static_cast<float>(accum[i].count);
      verts[i].Color.x = accum[i].r * inv;
      verts[i].Color.y = accum[i].g * inv;
      verts[i].Color.z = accum[i].b * inv;
      verts[i].Color.w = accum[i].a * inv;
    }
  }
}

XMFLOAT4 SurfaceRenderer::GetLandscapeColour(float height, float gradient,
                                              unsigned int x, unsigned int y,
                                              const LandscapeTexture* texture,
                                              float highest)
{
  // U: steepness — use abs(gradient) so both hemispheres map symmetrically.
  // |gradient| = 1 at poles (flat), 0 at equator (steep).
  float u = powf(1.0f - fabsf(gradient), 0.4f);

  // V: normalized height mapped to full sphere range [-highest, +highest] → [0, 1].
  // Maps top (height = highest) to v = 0 (peak), bottom (height = -highest) to v = 1.
  float v = (1.0f - height / highest) * 0.5f;

  // Darwinia-style deterministic noise from spatial position
  SeedRandom(x | y + Random());

  float absHeight = fabsf(height);
  v += SignedFrand(0.45f / (absHeight + 2.0f));

  // Map UV to pixel coordinates with clamping
  int px = static_cast<int>(u * texture->GetWidth());
  int py = static_cast<int>(v * texture->GetHeight());

  if (px < 0) px = 0;
  if (px > static_cast<int>(texture->GetWidth()) - 1)
    px = static_cast<int>(texture->GetWidth()) - 1;
  if (py < 0) py = 0;
  if (py > static_cast<int>(texture->GetHeight()) - 1)
    py = static_cast<int>(texture->GetHeight()) - 1;

  auto pixel = texture->GetPixel(px, py);

  // Convert from uint8 [0,255] to float [0,1]
  return {
    pixel.r / 255.0f,
    pixel.g / 255.0f,
    pixel.b / 255.0f,
    pixel.a / 255.0f
  };
}

// --- Darwinia-style deterministic random ---

void SurfaceRenderer::SeedRandom(unsigned int seed)
{
  m_rngState = seed;
  if (m_rngState == 0) m_rngState = 1; // avoid zero state
}

uint32_t SurfaceRenderer::Random()
{
  // Xorshift32
  m_rngState ^= m_rngState << 13;
  m_rngState ^= m_rngState >> 17;
  m_rngState ^= m_rngState << 5;
  return m_rngState;
}

float SurfaceRenderer::SignedFrand(float range)
{
  // Returns a value in [-range, +range]
  float t = static_cast<float>(Random()) / static_cast<float>(UINT32_MAX);
  return (t * 2.0f - 1.0f) * range;
}

uint64_t SurfaceRenderer::ComputeCacheKey(std::string_view meshKey,
                                          Neuron::SurfaceType surfaceType) noexcept
{
  // FNV-1a hash of the mesh key, combined with surface type
  uint32_t hash = 2166136261u;
  for (char c : meshKey)
  {
    hash ^= static_cast<uint32_t>(c);
    hash *= 16777619u;
  }
  return (static_cast<uint64_t>(hash) << 8) | static_cast<uint64_t>(surfaceType);
}
