#pragma once

#ifdef PROFILER_ENABLED
#include <pix3.h>
#endif

namespace Neuron
{
#ifdef PROFILER_ENABLED

  // CPU profiling events
  inline void ProfileBegin(std::wstring_view name, uint64_t color = PIX_COLOR_DEFAULT) noexcept { PIXBeginEvent(color, name.data()); }
  inline void ProfileEnd() noexcept { PIXEndEvent(); }
  inline void ProfileMarker(std::wstring_view name, uint64_t color = PIX_COLOR_DEFAULT) noexcept { PIXSetMarker(color, name.data()); }

  // GPU command-list-scoped profiling events
  inline void ProfileBegin(ID3D12GraphicsCommandList* cmdList, std::wstring_view name, uint64_t color = PIX_COLOR_DEFAULT) noexcept { PIXBeginEvent(cmdList, color, name.data()); }
  inline void ProfileEnd(ID3D12GraphicsCommandList* cmdList) noexcept { PIXEndEvent(cmdList); }
  inline void ProfileMarker(ID3D12GraphicsCommandList* cmdList, std::wstring_view name, uint64_t color = PIX_COLOR_DEFAULT) noexcept { PIXSetMarker(cmdList, color, name.data()); }

  // RAII scope guard for automatic begin/end pairing (CPU)
  class ProfileScope : NonCopyable
  {
  public:
    explicit ProfileScope(std::wstring_view name, uint64_t color = PIX_COLOR_DEFAULT) noexcept { ProfileBegin(name, color); }
    ~ProfileScope() noexcept { ProfileEnd(); }
  };

  // RAII scope guard for automatic begin/end pairing (GPU command list)
  class GpuProfileScope : NonCopyable
  {
  public:
    explicit GpuProfileScope(ID3D12GraphicsCommandList* cmdList, std::wstring_view name, uint64_t color = PIX_COLOR_DEFAULT) noexcept
      : m_cmdList(cmdList) { ProfileBegin(cmdList, name, color); }
    ~GpuProfileScope() noexcept { ProfileEnd(m_cmdList); }
  private:
    ID3D12GraphicsCommandList* m_cmdList;
  };

#define NEURON_PROFILE_CONCAT_INNER(a, b) a ## b
#define NEURON_PROFILE_CONCAT(a, b) NEURON_PROFILE_CONCAT_INNER(a, b)
#define PROFILE_FUNCTION() ::Neuron::ProfileScope NEURON_PROFILE_CONCAT(_pfScope_, __LINE__)(L"" __FUNCTIONW__)
#define PROFILE_SCOPE(name) ::Neuron::ProfileScope NEURON_PROFILE_CONCAT(_psScope_, __LINE__)(name)
#define GPU_PROFILE_SCOPE(cmdList, name) ::Neuron::GpuProfileScope NEURON_PROFILE_CONCAT(_gpsScope_, __LINE__)(cmdList, name)

#else

  // CPU no-ops
  constexpr void ProfileBegin([[maybe_unused]] std::wstring_view name, [[maybe_unused]] uint64_t color = 0) {}
  constexpr void ProfileEnd() {}
  constexpr void ProfileMarker([[maybe_unused]] std::wstring_view name, [[maybe_unused]] uint64_t color = 0) {}

  // GPU no-ops
  constexpr void ProfileBegin([[maybe_unused]] ID3D12GraphicsCommandList*, [[maybe_unused]] std::wstring_view name, [[maybe_unused]] uint64_t color = 0) {}
  constexpr void ProfileEnd([[maybe_unused]] ID3D12GraphicsCommandList*) {}
  constexpr void ProfileMarker([[maybe_unused]] ID3D12GraphicsCommandList*, [[maybe_unused]] std::wstring_view name, [[maybe_unused]] uint64_t color = 0) {}

#define PROFILE_FUNCTION()
#define PROFILE_SCOPE(name)
#define GPU_PROFILE_SCOPE(cmdList, name)

#endif
}
