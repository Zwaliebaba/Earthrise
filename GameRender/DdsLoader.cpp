#include "pch.h"
#include "DdsLoader.h"

using namespace Neuron::Graphics;

namespace
{
  constexpr uint32_t DDS_MAGIC = 0x20534444; // "DDS "

  // DDS pixel format flags
  constexpr uint32_t DDPF_FOURCC      = 0x4;
  constexpr uint32_t DDPF_RGB         = 0x40;
  constexpr uint32_t DDPF_ALPHAPIXELS = 0x1;

  struct DDS_PIXELFORMAT
  {
    uint32_t Size;
    uint32_t Flags;
    uint32_t FourCC;
    uint32_t RGBBitCount;
    uint32_t RBitMask;
    uint32_t GBitMask;
    uint32_t BBitMask;
    uint32_t ABitMask;
  };

  struct DDS_HEADER
  {
    uint32_t Size;
    uint32_t Flags;
    uint32_t Height;
    uint32_t Width;
    uint32_t PitchOrLinearSize;
    uint32_t Depth;
    uint32_t MipMapCount;
    uint32_t Reserved1[11];
    DDS_PIXELFORMAT PixelFormat;
    uint32_t Caps;
    uint32_t Caps2;
    uint32_t Caps3;
    uint32_t Caps4;
    uint32_t Reserved2;
  };

  constexpr uint32_t FOURCC_DXT1 = MAKEFOURCC('D', 'X', 'T', '1');
  constexpr uint32_t FOURCC_DXT3 = MAKEFOURCC('D', 'X', 'T', '3');
  constexpr uint32_t FOURCC_DXT5 = MAKEFOURCC('D', 'X', 'T', '5');

  // Decode a 16-bit RGB565 color to RGBA8888
  uint32_t DecodeRgb565(uint16_t c)
  {
    uint32_t r = ((c >> 11) & 0x1F) * 255 / 31;
    uint32_t g = ((c >> 5) & 0x3F) * 255 / 63;
    uint32_t b = (c & 0x1F) * 255 / 31;
    return r | (g << 8) | (b << 16) | 0xFF000000u;
  }

  // Interpolate two RGBA colors (component-wise)
  uint32_t LerpColor(uint32_t c0, uint32_t c1, uint32_t num, uint32_t den)
  {
    uint32_t r = (((c0 >> 0) & 0xFF) * (den - num) + ((c1 >> 0) & 0xFF) * num) / den;
    uint32_t g = (((c0 >> 8) & 0xFF) * (den - num) + ((c1 >> 8) & 0xFF) * num) / den;
    uint32_t b = (((c0 >> 16) & 0xFF) * (den - num) + ((c1 >> 16) & 0xFF) * num) / den;
    uint32_t a = (((c0 >> 24) & 0xFF) * (den - num) + ((c1 >> 24) & 0xFF) * num) / den;
    return r | (g << 8) | (b << 16) | (a << 24);
  }

  // Decompress a single BC1 (DXT1) 4x4 block
  void DecompressBC1Block(const uint8_t* block, uint32_t outColors[16])
  {
    uint16_t c0Raw = *reinterpret_cast<const uint16_t*>(block);
    uint16_t c1Raw = *reinterpret_cast<const uint16_t*>(block + 2);
    uint32_t indices = *reinterpret_cast<const uint32_t*>(block + 4);

    uint32_t c[4];
    c[0] = DecodeRgb565(c0Raw);
    c[1] = DecodeRgb565(c1Raw);

    if (c0Raw > c1Raw)
    {
      c[2] = LerpColor(c[0], c[1], 1, 3);
      c[3] = LerpColor(c[0], c[1], 2, 3);
    }
    else
    {
      c[2] = LerpColor(c[0], c[1], 1, 2);
      c[3] = 0; // transparent black
    }

    for (int i = 0; i < 16; ++i)
    {
      uint32_t idx = (indices >> (i * 2)) & 0x3;
      outColors[i] = c[idx];
    }
  }

  // Decompress a single BC3 (DXT5) 4x4 block
  void DecompressBC3Block(const uint8_t* block, uint32_t outColors[16])
  {
    // First 8 bytes: alpha block
    uint8_t a0 = block[0];
    uint8_t a1 = block[1];

    uint8_t alphas[8];
    alphas[0] = a0;
    alphas[1] = a1;

    if (a0 > a1)
    {
      alphas[2] = static_cast<uint8_t>((6 * a0 + 1 * a1) / 7);
      alphas[3] = static_cast<uint8_t>((5 * a0 + 2 * a1) / 7);
      alphas[4] = static_cast<uint8_t>((4 * a0 + 3 * a1) / 7);
      alphas[5] = static_cast<uint8_t>((3 * a0 + 4 * a1) / 7);
      alphas[6] = static_cast<uint8_t>((2 * a0 + 5 * a1) / 7);
      alphas[7] = static_cast<uint8_t>((1 * a0 + 6 * a1) / 7);
    }
    else
    {
      alphas[2] = static_cast<uint8_t>((4 * a0 + 1 * a1) / 5);
      alphas[3] = static_cast<uint8_t>((3 * a0 + 2 * a1) / 5);
      alphas[4] = static_cast<uint8_t>((2 * a0 + 3 * a1) / 5);
      alphas[5] = static_cast<uint8_t>((1 * a0 + 4 * a1) / 5);
      alphas[6] = 0;
      alphas[7] = 255;
    }

    // 48-bit alpha index block (6 bytes at block+2)
    uint64_t alphaBits = 0;
    for (int i = 0; i < 6; ++i)
      alphaBits |= static_cast<uint64_t>(block[2 + i]) << (i * 8);

    // Color block at block+8 (same as BC1)
    DecompressBC1Block(block + 8, outColors);

    // Apply decoded alpha to each pixel
    for (int i = 0; i < 16; ++i)
    {
      uint32_t alphaIdx = static_cast<uint32_t>((alphaBits >> (i * 3)) & 0x7);
      uint32_t pixel = outColors[i] & 0x00FFFFFFu;
      outColors[i] = pixel | (static_cast<uint32_t>(alphas[alphaIdx]) << 24);
    }
  }

  // Decompress a full BC1 or BC3 image
  void DecompressBlockCompressed(const uint8_t* src, uint32_t width, uint32_t height,
                                 uint32_t blockSize, bool isBC3,
                                 std::vector<uint32_t>& outPixels)
  {
    uint32_t blocksX = (width + 3) / 4;
    uint32_t blocksY = (height + 3) / 4;
    outPixels.resize(width * height);

    for (uint32_t by = 0; by < blocksY; ++by)
    {
      for (uint32_t bx = 0; bx < blocksX; ++bx)
      {
        uint32_t colors[16];
        if (isBC3)
          DecompressBC3Block(src, colors);
        else
          DecompressBC1Block(src, colors);

        src += blockSize;

        // Write 4x4 block to output
        for (uint32_t py = 0; py < 4; ++py)
        {
          uint32_t y = by * 4 + py;
          if (y >= height) break;
          for (uint32_t px = 0; px < 4; ++px)
          {
            uint32_t x = bx * 4 + px;
            if (x >= width) break;
            outPixels[y * width + x] = colors[py * 4 + px];
          }
        }
      }
    }
  }

  // Channel shift for a given bitmask
  int MaskShift(uint32_t mask)
  {
    if (mask == 0) return 0;
    int shift = 0;
    while ((mask & 1) == 0) { mask >>= 1; ++shift; }
    return shift;
  }
}

CpuImage DdsLoader::LoadFromFile(const std::wstring& filePath)
{
  CpuImage image;

  // Read entire file
  HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                             nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hFile == INVALID_HANDLE_VALUE)
  {
    std::string narrow(filePath.size(), '\0');
    for (size_t i = 0; i < filePath.size(); ++i)
      narrow[i] = static_cast<char>(filePath[i]);
    Neuron::DebugTrace("DdsLoader: failed to open {}", narrow);
    return image;
  }

  LARGE_INTEGER fileSize;
  GetFileSizeEx(hFile, &fileSize);
  std::vector<uint8_t> fileData(static_cast<size_t>(fileSize.QuadPart));

  DWORD bytesRead = 0;
  BOOL ok = ReadFile(hFile, fileData.data(), static_cast<DWORD>(fileData.size()), &bytesRead, nullptr);
  CloseHandle(hFile);

  if (!ok || bytesRead < sizeof(uint32_t) + sizeof(DDS_HEADER))
  {
    Neuron::DebugTrace("DdsLoader: file too small or read failed");
    return image;
  }

  const uint8_t* ptr = fileData.data();

  // Validate magic
  uint32_t magic = *reinterpret_cast<const uint32_t*>(ptr);
  if (magic != DDS_MAGIC)
  {
    Neuron::DebugTrace("DdsLoader: invalid DDS magic");
    return image;
  }
  ptr += sizeof(uint32_t);

  const auto& header = *reinterpret_cast<const DDS_HEADER*>(ptr);
  ptr += sizeof(DDS_HEADER);

  image.Width = header.Width;
  image.Height = header.Height;

  const auto& pf = header.PixelFormat;

  if (pf.Flags & DDPF_FOURCC)
  {
    // Block-compressed format
    if (pf.FourCC == FOURCC_DXT1)
    {
      DecompressBlockCompressed(ptr, image.Width, image.Height, 8, false, image.Pixels);
    }
    else if (pf.FourCC == FOURCC_DXT3 || pf.FourCC == FOURCC_DXT5)
    {
      DecompressBlockCompressed(ptr, image.Width, image.Height, 16, true, image.Pixels);
    }
    else
    {
      Neuron::DebugTrace("DdsLoader: unsupported FourCC format 0x{:08X}", pf.FourCC);
    }
  }
  else if (pf.Flags & DDPF_RGB)
  {
    // Uncompressed RGB(A)
    uint32_t bpp = pf.RGBBitCount / 8;
    bool hasAlpha = (pf.Flags & DDPF_ALPHAPIXELS) != 0;

    int rShift = MaskShift(pf.RBitMask);
    int gShift = MaskShift(pf.GBitMask);
    int bShift = MaskShift(pf.BBitMask);
    int aShift = hasAlpha ? MaskShift(pf.ABitMask) : 0;

    image.Pixels.resize(image.Width * image.Height);

    for (uint32_t y = 0; y < image.Height; ++y)
    {
      for (uint32_t x = 0; x < image.Width; ++x)
      {
        uint32_t raw = 0;
        memcpy(&raw, ptr, bpp);
        ptr += bpp;

        uint32_t r = (raw & pf.RBitMask) >> rShift;
        uint32_t g = (raw & pf.GBitMask) >> gShift;
        uint32_t b = (raw & pf.BBitMask) >> bShift;
        uint32_t a = hasAlpha ? ((raw & pf.ABitMask) >> aShift) : 255;

        // Output as RGBA8888 (R in low byte)
        image.Pixels[y * image.Width + x] = r | (g << 8) | (b << 16) | (a << 24);
      }
    }
  }
  else
  {
    Neuron::DebugTrace("DdsLoader: unsupported pixel format flags 0x{:08X}", pf.Flags);
  }

  return image;
}
