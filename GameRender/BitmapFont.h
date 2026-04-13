#pragma once

#include "PipelineHelpers.h"
#include "ConstantBufferAllocator.h"
#include "ShaderVisibleHeap.h"

namespace Neuron::Graphics
{
  // Bitmap font renderer using a DDS glyph atlas.
  // Atlas layout: 16×14 grid of 16×16 pixel cells, covering ASCII 32–255.
  // Each font DDS is language-specific (e.g., EditorFont-ENG.dds).
  //
  // Usage:
  //   font.LoadFromFile(L"Fonts\\EditorFont-ENG.dds");
  //   font.BeginDraw(cmdList, cbAlloc, srvHeap, screenW, screenH);
  //   font.DrawString(10, 10, "Hello", color);
  //   font.EndDraw();
  class BitmapFont
  {
  public:
    // Load the glyph atlas from a DDS file under GameData/.
    void LoadFromFile(const std::wstring& relativePath, ShaderVisibleHeap& _srvHeap);

    // Begin a text rendering batch. Binds pipeline and projection.
    void BeginDraw(ID3D12GraphicsCommandList* cmdList,
      ConstantBufferAllocator& cbAlloc,
      ShaderVisibleHeap& srvHeap,
      UINT screenWidth, UINT screenHeight);

    // Queue a string for rendering. Call between BeginDraw/EndDraw.
    void XM_CALLCONV DrawString(float x, float y, std::string_view text,
      FXMVECTOR color, float scale = 1.0f);

    // Flush and submit all queued glyphs.
    void EndDraw();

    // Metrics (in texels, before scaling).
    [[nodiscard]] float GetGlyphWidth()  const noexcept { return static_cast<float>(GLYPH_WIDTH); }
    [[nodiscard]] float GetGlyphHeight() const noexcept { return static_cast<float>(GLYPH_HEIGHT); }

    // Measure the width of a string in pixels at the given scale.
    [[nodiscard]] float MeasureString(std::string_view text, float scale = 1.0f) const noexcept;

    [[nodiscard]] bool IsLoaded() const noexcept { return m_pso != nullptr; }

  private:
    // Atlas constants
    static constexpr uint32_t GLYPH_WIDTH  = 16;
    static constexpr uint32_t GLYPH_HEIGHT = 16;
    static constexpr uint32_t COLUMNS      = 16;
    static constexpr uint32_t ROWS         = 14;
    static constexpr uint32_t FIRST_CHAR   = 32; // ASCII space

    // Vertex format: screen position + UV + tint color
    struct FontVertex
    {
      XMFLOAT2 Position;
      XMFLOAT2 TexCoord;
      XMFLOAT4 Color;
    };

    void FlushBatch();
    void CreatePipeline();

    // GPU resources
    com_ptr<ID3D12Resource>      m_texture;
    D3D12_CPU_DESCRIPTOR_HANDLE  m_srvCPU{};
    D3D12_GPU_DESCRIPTOR_HANDLE  m_srvGPU{};  // Persistent slot in shader-visible heap

    // Pipeline (shared across all BitmapFont instances via static init)
    static com_ptr<ID3D12PipelineState>  s_pso;
    static com_ptr<ID3D12RootSignature>  s_rootSignature;
    static bool s_pipelineReady;

    // Alias for instance use
    ID3D12PipelineState*  m_pso = nullptr;
    ID3D12RootSignature*  m_rootSignature = nullptr;

    // Per-draw state
    ID3D12GraphicsCommandList* m_cmdList = nullptr;
    ConstantBufferAllocator*   m_cbAlloc = nullptr;
    ShaderVisibleHeap*         m_srvHeap = nullptr;

    // Atlas dimensions (from DDS header)
    uint32_t m_texWidth  = 0;
    uint32_t m_texHeight = 0;

    // Glyph batch
    std::vector<FontVertex> m_vertices;
    static constexpr uint32_t MAX_GLYPHS_PER_BATCH = 1024;
    static constexpr uint32_t VERTICES_PER_GLYPH = 6;
  };
}
