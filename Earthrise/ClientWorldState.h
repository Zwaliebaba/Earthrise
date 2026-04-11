#pragma once

#include "GameTypes/EntityHandle.h"
#include "GameTypes/SpaceObjectCategory.h"
#include "Messages.h"

// ClientWorldState — maintains a local mirror of server-authoritative entities.
// Applies spawn/despawn messages and state snapshots from the server.
// Provides interpolated entity transforms for smooth rendering between server ticks.

struct ClientEntity
{
  Neuron::EntityHandle Handle;
  Neuron::SpaceObjectCategory Category = Neuron::SpaceObjectCategory::SpaceObject;
  uint32_t MeshHash = 0;
  bool Active = false;

  // Current interpolated state (used for rendering)
  XMFLOAT3 Position = {};
  XMFLOAT4 Orientation = { 0, 0, 0, 1 };
  XMFLOAT4 Color = { 1, 1, 1, 1 };

  // Velocity from last snapshot (for dead reckoning / prediction)
  XMFLOAT3 Velocity = {};

  // Previous snapshot state (for interpolation)
  XMFLOAT3 PrevPosition = {};
  XMFLOAT4 PrevOrientation = { 0, 0, 0, 1 };

  // Target snapshot state (interpolation target)
  XMFLOAT3 TargetPosition = {};
  XMFLOAT4 TargetOrientation = { 0, 0, 0, 1 };

  // Mesh key string for MeshCache lookup (resolved from MeshHash + Category)
  std::string MeshKey;
};

class ClientWorldState
{
public:
  ClientWorldState() = default;

  // Apply an entity spawn message from the server.
  void ApplySpawn(const Neuron::EntitySpawnMsg& _msg);

  // Apply an entity despawn message from the server.
  void ApplyDespawn(const Neuron::EntityDespawnMsg& _msg);

  // Apply a state snapshot from the server. Updates interpolation targets.
  void ApplySnapshot(const Neuron::StateSnapshotMsg& _msg);

  // Advance interpolation by _deltaT seconds.
  // Call once per frame before rendering.
  void Update(float _deltaT);

  // Lookup entity by handle. Returns nullptr if not found.
  [[nodiscard]] const ClientEntity* GetEntity(Neuron::EntityHandle _handle) const;
  [[nodiscard]] ClientEntity* GetEntity(Neuron::EntityHandle _handle);

  // Iterate all active entities.
  template <typename Func>
  void ForEachActive(Func&& _func) const
  {
    for (const auto& [id, entity] : m_entities)
    {
      if (entity.Active)
        _func(entity);
    }
  }

  template <typename Func>
  void ForEachActive(Func&& _func)
  {
    for (auto& [id, entity] : m_entities)
    {
      if (entity.Active)
        _func(entity);
    }
  }

  // Number of active entities.
  [[nodiscard]] uint32_t ActiveCount() const noexcept { return m_activeCount; }

  // The last server tick received.
  [[nodiscard]] uint32_t LastServerTick() const noexcept { return m_lastServerTick; }

  // Clear all entities.
  void Clear();

  // Resolve MeshHash to MeshKey (category folder + mesh name).
  // Uses a reverse lookup table populated from mesh cache.
  void RegisterMeshName(uint32_t _hash, std::string_view _key);

private:
  // Entities keyed by handle raw ID for O(1) lookup.
  std::unordered_map<uint32_t, ClientEntity> m_entities;

  // MeshHash → MeshKey reverse lookup.
  std::unordered_map<uint32_t, std::string> m_meshHashToKey;

  uint32_t m_activeCount = 0;
  uint32_t m_lastServerTick = 0;

  // Interpolation state
  float m_interpAlpha = 0.0f;       // 0..1 progress between prev→target
  static constexpr float SNAPSHOT_INTERVAL = 1.0f / 20.0f; // 20 Hz server tick
};
