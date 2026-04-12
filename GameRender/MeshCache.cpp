#include "pch.h"
#include "MeshCache.h"
#include "CmoLoader.h"

using namespace Neuron::Graphics;

uint32_t MeshCache::HashKey(std::string_view key) noexcept
{
  // FNV-1a hash (same as Neuron::HashMeshName in ObjectDefs.h)
  uint32_t hash = 2166136261u;
  for (char c : key)
  {
    hash ^= static_cast<uint32_t>(c);
    hash *= 16777619u;
  }
  return hash;
}

const char* MeshCache::GetCategoryFolder(SpaceObjectCategory category) noexcept
{
  switch (category)
  {
  case SpaceObjectCategory::Asteroid:    return "Asteroids";
  case SpaceObjectCategory::Crate:       return "Crates";
  case SpaceObjectCategory::Decoration:  return "Decorations";
  case SpaceObjectCategory::Ship:        return "Hulls";
  case SpaceObjectCategory::Jumpgate:    return "Jumpgates";
  case SpaceObjectCategory::Projectile:  return "Projectiles";
  case SpaceObjectCategory::SpaceObject: return "SpaceObjects";
  case SpaceObjectCategory::Station:     return "Stations";
  case SpaceObjectCategory::Turret:      return "Turrets";
  case SpaceObjectCategory::Planet:      return "Planets";
  case SpaceObjectCategory::Sun:         return "Suns";
  default:                               return "";
  }
}

Mesh* MeshCache::GetMesh(std::string_view key)
{
  const uint32_t hash = HashKey(key);

  auto it = m_cache.find(hash);
  if (it != m_cache.end())
    return it->second.get();

  // Build file path: <HomeDir>/Meshes/<key>.cmo
  std::wstring filePath = FileSys::GetHomeDirectory();
  filePath += L"Meshes\\";

  // Convert key (narrow) to wide
  for (char c : key)
    filePath += static_cast<wchar_t>(c == '/' ? '\\' : c);
  filePath += L".cmo";

  // Check if file exists
  if (GetFileAttributesW(filePath.c_str()) == INVALID_FILE_ATTRIBUTES)
  {
    Neuron::DebugTrace("MeshCache: file not found — {}", std::string(key));
    return nullptr;
  }

  auto meshDataVec = CmoLoader::LoadFromFile(filePath);
  if (meshDataVec.empty())
  {
    Neuron::DebugTrace("MeshCache: no meshes in file — {}", std::string(key));
    return nullptr;
  }

  // Use the first mesh in the file
  auto mesh = std::make_unique<Mesh>();
  mesh->CreateFromCmoData(meshDataVec[0]);

  auto* ptr = mesh.get();
  m_cache[hash] = std::move(mesh);
  return ptr;
}

void MeshCache::PreloadCategory(SpaceObjectCategory category)
{
  const char* folder = GetCategoryFolder(category);
  if (!folder[0]) return;

  std::wstring searchPath = FileSys::GetHomeDirectory();
  searchPath += L"Meshes\\";

  // Convert folder to wide
  std::wstring wideFolder;
  for (const char* p = folder; *p; ++p)
    wideFolder += static_cast<wchar_t>(*p);
  searchPath += wideFolder;
  searchPath += L"\\*.cmo";

  WIN32_FIND_DATAW findData;
  HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
  if (hFind == INVALID_HANDLE_VALUE)
    return;

  do
  {
    // Build key: "Folder/FileName" (without .cmo extension)
    std::wstring name(findData.cFileName);
    if (name.size() > 4) // strip ".cmo"
      name.resize(name.size() - 4);

    // Convert to narrow key
    std::string key(folder);
    key += '/';
    for (wchar_t wc : name)
      key += static_cast<char>(wc);

    GetMesh(key);
  }
  while (FindNextFileW(hFind, &findData));

  FindClose(hFind);
}

void MeshCache::BuildMeshHashMap(SpaceObjectCategory category,
                                  std::unordered_map<uint32_t, std::string>& _outMap)
{
  const char* folder = GetCategoryFolder(category);
  if (!folder[0]) return;

  std::wstring searchPath = FileSys::GetHomeDirectory();
  searchPath += L"Meshes\\";

  std::wstring wideFolder;
  for (const char* p = folder; *p; ++p)
    wideFolder += static_cast<wchar_t>(*p);
  searchPath += wideFolder;
  searchPath += L"\\*.cmo";

  WIN32_FIND_DATAW findData;
  HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
  if (hFind == INVALID_HANDLE_VALUE)
    return;

  do
  {
    std::wstring name(findData.cFileName);
    if (name.size() > 4)
      name.resize(name.size() - 4);

    // Bare mesh name (what the server hashes)
    std::string bareName;
    for (wchar_t wc : name)
      bareName += static_cast<char>(wc);

    // Full cache key: "Category/BareName"
    std::string fullKey(folder);
    fullKey += '/';
    fullKey += bareName;

    // Hash the bare name (same as Neuron::HashMeshName)
    uint32_t bareHash = HashKey(bareName);
    _outMap[bareHash] = fullKey;
  }
  while (FindNextFileW(hFind, &findData));

  FindClose(hFind);
}
