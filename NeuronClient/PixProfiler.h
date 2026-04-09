#pragma once

#include <pix3.h>

namespace Neuron
{
#ifdef PROFILER_ENABLED
  inline void ProfileStart(const std::wstring_view _itemName) { PIXBeginEvent(PIX_COLOR_DEFAULT, _itemName.data()); }
  inline void ProfileEnd() { PIXEndEvent(); }
  inline void ProfileSetMarker(const std::wstring_view _itemName) { PIXSetMarker(PIX_COLOR_DEFAULT, _itemName.data()); }
#else
  constexpr void StartProfile([[maybe_unused]] std::wstring_view _itemName) {}
  constexpr void EndProfile() {}
  constexpr void SetMarker([[maybe_unused]] const std::wstring_view _itemName) {}
#endif
}
