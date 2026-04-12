#pragma once

#include <random>

namespace Neuron
{
  // NeuronRandom — thin PRNG wrapper around std::mt19937 with game-friendly helpers.
  // Deterministic given the same seed — suitable for procedural generation.
  class NeuronRandom
  {
  public:
    explicit NeuronRandom(uint64_t _seed = 0) noexcept
      : m_rng(static_cast<std::mt19937::result_type>(_seed))
    {
    }

    void Seed(uint64_t _seed) noexcept
    {
      m_rng.seed(static_cast<std::mt19937::result_type>(_seed));
    }

    // Uniform float in [_min, _max].
    [[nodiscard]] float FloatRange(float _min, float _max) noexcept
    {
      std::uniform_real_distribution<float> dist(_min, _max);
      return dist(m_rng);
    }

    // Uniform int in [_min, _max].
    [[nodiscard]] int IntRange(int _min, int _max) noexcept
    {
      std::uniform_int_distribution<int> dist(_min, _max);
      return dist(m_rng);
    }

    // Uniform uint32_t in [_min, _max].
    [[nodiscard]] uint32_t UintRange(uint32_t _min, uint32_t _max) noexcept
    {
      std::uniform_int_distribution<uint32_t> dist(_min, _max);
      return dist(m_rng);
    }

    // Uniform float in [0, 1).
    [[nodiscard]] float Unit() noexcept
    {
      std::uniform_real_distribution<float> dist(0.0f, 1.0f);
      return dist(m_rng);
    }

    // Random unit vector on the unit sphere (uniform distribution).
    [[nodiscard]] XMFLOAT3 UnitSphere() noexcept
    {
      float z = FloatRange(-1.0f, 1.0f);
      float phi = FloatRange(0.0f, XM_2PI);
      float r = std::sqrtf(1.0f - z * z);
      return { r * std::cosf(phi), r * std::sinf(phi), z };
    }

    // Random angle in [0, 2π).
    [[nodiscard]] float Angle() noexcept
    {
      return FloatRange(0.0f, XM_2PI);
    }

    // Access the underlying engine (for algorithms needing std::mt19937 directly).
    [[nodiscard]] std::mt19937& Engine() noexcept { return m_rng; }

  private:
    std::mt19937 m_rng;
  };
}
