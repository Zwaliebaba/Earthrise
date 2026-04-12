#pragma once

namespace Neuron::Graphics
{
  // CPU-side image data decoded from a DDS file.
  struct CpuImage
  {
    uint32_t Width  = 0;
    uint32_t Height = 0;
    std::vector<uint32_t> Pixels; // RGBA8888 (R in low byte)
  };

  // Minimal DDS file parser for CPU-side pixel sampling.
  // Supports uncompressed R8G8B8A8, B8G8R8A8, B8G8R8X8, and
  // block-compressed BC1 (DXT1) and BC3 (DXT5) formats.
  namespace DdsLoader
  {
    [[nodiscard]] CpuImage LoadFromFile(const std::wstring& filePath);
  }
}
