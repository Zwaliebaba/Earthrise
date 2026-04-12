#pragma once

// CMO (VSD3DStarter) mesh file parser.
// Extracts vertex positions, normals, and indices from .cmo binary format.
// Ignores texture references and baked material colors (colors assigned at runtime).

namespace Neuron::Graphics
{
  // Simplified vertex for flat-color rendering (position + normal only).
  struct FlatColorVertex
  {
    XMFLOAT3 Position;
    XMFLOAT3 Normal;
  };

  // Submesh: a range of indices sharing a material.
  struct CmoSubmesh
  {
    uint32_t MaterialIndex;
    uint32_t StartIndex;
    uint32_t IndexCount;  // primCount * 3
  };

  // Parsed mesh data (CPU-side, no GPU resources).
  struct CmoMeshData
  {
    std::wstring Name;
    std::vector<FlatColorVertex> Vertices;
    std::vector<XMFLOAT2> TexCoords; // Per-vertex UVs (parallel to Vertices)
    std::vector<uint16_t> Indices;
    std::vector<CmoSubmesh> Submeshes;
  };

  namespace CmoLoader
  {
    // Parse a .cmo file and return all meshes contained within.
    [[nodiscard]] std::vector<CmoMeshData> LoadFromFile(const std::wstring& filePath);
    [[nodiscard]] std::vector<CmoMeshData> LoadFromMemory(const uint8_t* data, size_t size);
  }

  // Procedural mesh generators — produce CmoMeshData without loading from disk.
  namespace ProceduralMesh
  {
    // Generate a UV sphere with unit radius. rings = latitude bands, slices = longitude bands.
    [[nodiscard]] CmoMeshData GenerateUVSphere(uint32_t rings, uint32_t slices);
  }
}
