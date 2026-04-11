#pragma once

#include "GameTypes/EntityHandle.h"

// Prediction.h — client-side prediction for the local player's fleet.
// Immediately shows movement toward a command target; reconciles with server corrections.

struct PredictionEntry
{
  Neuron::EntityHandle Handle;
  XMFLOAT3 CommandTarget = {};      // The pending move-to destination
  bool HasPendingCommand = false;    // Whether we have an unconfirmed command
  XMFLOAT3 PredictedPosition = {};  // Locally predicted position
};

class Prediction
{
public:
  Prediction() = default;

  // Record a local move command for a fleet ship.
  void ApplyLocalCommand(Neuron::EntityHandle _handle,
    const XMFLOAT3& _currentPos, const XMFLOAT3& _target);

  // Update predictions by _deltaT seconds (move predicted positions toward targets).
  void Update(float _deltaT, float _moveSpeed);

  // Reconcile with server correction. If server position is close enough to predicted,
  // accept smoothly. If too far, snap to server position.
  void Reconcile(Neuron::EntityHandle _handle,
    const XMFLOAT3& _serverPos, float _snapThreshold = 5.0f);

  // Get predicted position for a handle, or nullptr if no prediction is active.
  [[nodiscard]] const XMFLOAT3* GetPredictedPosition(Neuron::EntityHandle _handle) const;

  // Remove prediction entry.
  void Remove(Neuron::EntityHandle _handle);

  // Clear all predictions.
  void Clear();

  // Check if a handle has an active prediction.
  [[nodiscard]] bool HasPrediction(Neuron::EntityHandle _handle) const;

private:
  std::unordered_map<uint32_t, PredictionEntry> m_entries;
};
