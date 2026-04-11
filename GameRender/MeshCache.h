#pragma once

#include "Mesh.h"
#include "GameTypes/SpaceObjectCategory.h"

namespace Neuron::Graphics
{
  // Load-once asset cache for meshes, keyed by relative path (e.g., "Hulls/HullAvalanche").
  // Resolves to GameData/Meshes/<key>.cmo on disk.
  class MeshCache
  {
  public:
    MeshCache() = default;
    ~MeshCache() = default;

    MeshCache(const MeshCache&) = delete;
    MeshCache& operator=(const MeshCache&) = delete;

    // Get (or load) a mesh by key. Returns nullptr if the file doesn't exist.
    Mesh* GetMesh(std::string_view key);

    // Batch-load all meshes in a category folder.
    void PreloadCategory(SpaceObjectCategory category);

    // Number of cached meshes.
    [[nodiscard]] size_t GetCacheSize() const noexcept { return m_cache.size(); }

    // Get the subfolder name for a category.
    [[nodiscard]] static const char* GetCategoryFolder(SpaceObjectCategory category) noexcept;

  private:
    // Key → Mesh, using FNV-1a hash of the key string
    std::unordered_map<uint32_t, std::unique_ptr<Mesh>> m_cache;

    [[nodiscard]] static uint32_t HashKey(std::string_view key) noexcept;
  };
}
