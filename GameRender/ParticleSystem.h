#pragma once

// ParticleSystem — CPU-side particle emitter and simulation.
// Maintains a pool of flat-color particles (position, velocity, color, lifetime).
// Rendering is handled by ParticleRenderer which reads the particle buffer.

#include <vector>

namespace Neuron::Graphics
{
  struct Particle
  {
    XMFLOAT3 Position{};
    XMFLOAT3 Velocity{};
    XMFLOAT4 Color{ 1, 1, 1, 1 };
    float    Age      = 0.0f;
    float    Lifetime = 1.0f;
    float    Size     = 0.5f;
  };

  class ParticleSystem
  {
  public:
    static constexpr uint32_t MAX_PARTICLES = 4096;

    ParticleSystem() = default;

    // Simulate all particles: integrate velocity, age, remove dead.
    void Update(float _deltaTime);

    // Spawn a burst of particles at a position with random velocity spread.
    void SpawnBurst(const XMFLOAT3& _center, float _radius,
                    const XMFLOAT4& _color, uint32_t _count);

    // Spawn a single particle (for continuous emitters like engine exhaust).
    void SpawnParticle(const Particle& _particle);

    // Read-only access to the live particle buffer for rendering.
    [[nodiscard]] const std::vector<Particle>& GetParticles() const noexcept { return m_particles; }

    // Number of live particles.
    [[nodiscard]] uint32_t ActiveCount() const noexcept { return static_cast<uint32_t>(m_particles.size()); }

    // Remove all particles.
    void Clear() noexcept { m_particles.clear(); }

  private:
    std::vector<Particle> m_particles;

    // Simple deterministic pseudo-random for particle velocity spread.
    uint32_t m_rngState = 0x12345678u;
    [[nodiscard]] float RandomFloat();  // [0, 1)
    [[nodiscard]] float RandomRange(float _min, float _max);
  };
}
