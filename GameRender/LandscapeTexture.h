#pragma once

#include "DdsLoader.h"
#include "GameTypes/SurfaceType.h"

namespace Neuron::Graphics
{
  // CPU-side landscape gradient texture for per-vertex color sampling.
  // Small textures (typically 256x256) loaded once per SurfaceType.
  class LandscapeTexture
  {
  public:
    static std::unique_ptr<LandscapeTexture> LoadFromFile(const std::wstring& path);

    struct RGBAPixel
    {
      uint8_t r, g, b, a;
    };

    [[nodiscard]] RGBAPixel GetPixel(int x, int y) const noexcept;
    [[nodiscard]] uint32_t GetWidth() const noexcept { return m_width; }
    [[nodiscard]] uint32_t GetHeight() const noexcept { return m_height; }

    // Get the DDS filename for a surface type (e.g., "LandscapeDefault.dds")
    [[nodiscard]] static const wchar_t* GetTextureFilename(Neuron::SurfaceType type) noexcept;

  private:
    uint32_t m_width  = 0;
    uint32_t m_height = 0;
    std::vector<RGBAPixel> m_pixels;
  };
}
