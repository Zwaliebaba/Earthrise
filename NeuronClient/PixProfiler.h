#pragma once

#ifdef PROFILER_ENABLED
#include <pix3.h>
#endif

namespace Neuron
{
#ifdef PROFILER_ENABLED

  // CPU profiling events
  inline void ProfileBegin(std::wstring_view name, UINT64 color = PIX_COLOR_DEFAULT) { PIXBeginEvent(color, name.data()); }
  inline void ProfileEnd() { PIXEndEvent(); }
  inline void ProfileMarker(std::wstring_view name, UINT64 color = PIX_COLOR_DEFAULT) { PIXSetMarker(color, name.data()); }

  // GPU command-list-scoped profiling events
  inline void ProfileBegin(ID3D12GraphicsCommandList* cmdList, std::wstring_view name, UINT64 color = PIX_COLOR_DEFAULT) { PIXBeginEvent(cmdList, color, name.data()); }
  inline void ProfileEnd(ID3D12GraphicsCommandList* cmdList) { PIXEndEvent(cmdList); }
  inline void ProfileMarker(ID3D12GraphicsCommandList* cmdList, std::wstring_view name, UINT64 color = PIX_COLOR_DEFAULT) { PIXSetMarker(cmdList, color, name.data()); }

  // RAII scope guard for automatic begin/end pairing (CPU)
  class ProfileScope
  {
  public:
    explicit ProfileScope(std::wstring_view name, UINT64 color = PIX_COLOR_DEFAULT) { ProfileBegin(name, color); }
    ~ProfileScope() { ProfileEnd(); }
    ProfileScope(const ProfileScope&) = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;
  };

  // RAII scope guard for automatic begin/end pairing (GPU command list)
  class GpuProfileScope
  {
  public:
    explicit GpuProfileScope(ID3D12GraphicsCommandList* cmdList, std::wstring_view name, UINT64 color = PIX_COLOR_DEFAULT)
      : m_cmdList(cmdList) { ProfileBegin(cmdList, name, color); }
    ~GpuProfileScope() { ProfileEnd(m_cmdList); }
    GpuProfileScope(const GpuProfileScope&) = delete;
    GpuProfileScope& operator=(const GpuProfileScope&) = delete;
  private:
    ID3D12GraphicsCommandList* m_cmdList;
  };

#define PROFILE_FUNCTION() ::Neuron::ProfileScope NEURON_PROFILE_CONCAT(_pfScope_, __LINE__)(L"" __FUNCTIONW__)
#define PROFILE_SCOPE(name) ::Neuron::ProfileScope NEURON_PROFILE_CONCAT(_psScope_, __LINE__)(name)
#define GPU_PROFILE_SCOPE(cmdList, name) ::Neuron::GpuProfileScope NEURON_PROFILE_CONCAT(_gpsScope_, __LINE__)(cmdList, name)
#define NEURON_PROFILE_CONCAT_INNER(a, b) a ## b
#define NEURON_PROFILE_CONCAT(a, b) NEURON_PROFILE_CONCAT_INNER(a, b)

#else

  constexpr void ProfileBegin([[maybe_unused]] std::wstring_view name, [[maybe_unused]] uint64_t color = 0) {}
  constexpr void ProfileEnd() {}
  constexpr void ProfileMarker([[maybe_unused]] std::wstring_view name, [[maybe_unused]] uint64_t color = 0) {}

#define PROFILE_FUNCTION()
#define PROFILE_SCOPE(name)
#define GPU_PROFILE_SCOPE(cmdList, name)

#endif
}
