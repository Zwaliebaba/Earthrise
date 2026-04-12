#include "pch.h"
#include "ParticleSystem.h"

using namespace Neuron::Graphics;

void ParticleSystem::Update(float _deltaTime)
{
  for (auto& p : m_particles)
  {
    // Integrate position
    p.Position.x += p.Velocity.x * _deltaTime;
    p.Position.y += p.Velocity.y * _deltaTime;
    p.Position.z += p.Velocity.z * _deltaTime;

    // Age
    p.Age += _deltaTime;

    // Fade alpha linearly over lifetime
    float t = p.Age / p.Lifetime;
    p.Color.w = std::max(0.0f, 1.0f - t);
  }

  // Remove dead particles (age >= lifetime)
  std::erase_if(m_particles, [](const Particle& p) { return p.Age >= p.Lifetime; });
}

void ParticleSystem::SpawnBurst(const XMFLOAT3& _center, float _radius,
                                 const XMFLOAT4& _color, uint32_t _count)
{
  for (uint32_t i = 0; i < _count; ++i)
  {
    if (m_particles.size() >= MAX_PARTICLES) break;

    Particle p;
    p.Position = _center;
    p.Color    = _color;
    p.Lifetime = RandomRange(0.3f, 1.2f);
    p.Size     = RandomRange(0.1f, 0.4f) * _radius;
    p.Age      = 0.0f;

    // Random velocity in a sphere
    float speed = RandomRange(1.0f, 6.0f) * _radius;
    float theta = RandomRange(0.0f, XM_2PI);
    float phi   = RandomRange(-XM_PIDIV2, XM_PIDIV2);
    float cosPhi = cosf(phi);
    p.Velocity.x = cosf(theta) * cosPhi * speed;
    p.Velocity.y = sinf(phi) * speed;
    p.Velocity.z = sinf(theta) * cosPhi * speed;

    m_particles.push_back(p);
  }
}

void ParticleSystem::SpawnParticle(const Particle& _particle)
{
  if (m_particles.size() >= MAX_PARTICLES) return;
  m_particles.push_back(_particle);
}

float ParticleSystem::RandomFloat()
{
  // Xorshift32
  m_rngState ^= m_rngState << 13;
  m_rngState ^= m_rngState >> 17;
  m_rngState ^= m_rngState << 5;
  return static_cast<float>(m_rngState) / static_cast<float>(UINT32_MAX);
}

float ParticleSystem::RandomRange(float _min, float _max)
{
  return _min + RandomFloat() * (_max - _min);
}
