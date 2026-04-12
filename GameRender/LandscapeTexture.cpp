#include "pch.h"
#include "LandscapeTexture.h"

using namespace Neuron::Graphics;

const wchar_t* LandscapeTexture::GetTextureFilename(Neuron::SurfaceType type) noexcept
{
  switch (type)
  {
  case Neuron::SurfaceType::Default:     return L"LandscapeDefault.dds";
  case Neuron::SurfaceType::IceCap:      return L"LandscapeIcecaps.dds";
  case Neuron::SurfaceType::Containment: return L"LandscapeContainment.dds";
  case Neuron::SurfaceType::Desert:      return L"LandscapeDesert.dds";
  case Neuron::SurfaceType::Earth:       return L"LandscapeEarth.dds";
  default:                               return L"LandscapeDefault.dds";
  }
}

std::unique_ptr<LandscapeTexture> LandscapeTexture::LoadFromFile(const std::wstring& path)
{
  CpuImage image = DdsLoader::LoadFromFile(path);
  if (image.Width == 0 || image.Height == 0 || image.Pixels.empty())
    return nullptr;

  auto tex = std::make_unique<LandscapeTexture>();
  tex->m_width = image.Width;
  tex->m_height = image.Height;
  tex->m_pixels.resize(image.Pixels.size());

  // Convert from packed uint32_t RGBA to RGBAPixel
  for (size_t i = 0; i < image.Pixels.size(); ++i)
  {
    uint32_t p = image.Pixels[i];
    tex->m_pixels[i].r = static_cast<uint8_t>((p >> 0) & 0xFF);
    tex->m_pixels[i].g = static_cast<uint8_t>((p >> 8) & 0xFF);
    tex->m_pixels[i].b = static_cast<uint8_t>((p >> 16) & 0xFF);
    tex->m_pixels[i].a = static_cast<uint8_t>((p >> 24) & 0xFF);
  }

  return tex;
}

LandscapeTexture::RGBAPixel LandscapeTexture::GetPixel(int x, int y) const noexcept
{
  ASSERT(x >= 0 && x < static_cast<int>(m_width));
  ASSERT(y >= 0 && y < static_cast<int>(m_height));
  return m_pixels[y * m_width + x];
}
