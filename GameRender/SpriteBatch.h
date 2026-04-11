#pragma once

#include "PipelineHelpers.h"
#include "ConstantBufferAllocator.h"
#include "UploadHeap.h"

namespace Neuron::Graphics
{
  // Minimal DX12 sprite batcher for textured quads (text glyphs, icons).
  // Supports Begin()/Draw()/End() API with per-sprite transforms.
  // Used by Phase 8 for HUD/UI rendering via RenderCanvas().
  class SpriteBatch
  {
  public:
    void Initialize();

    // Begin a sprite batch. Sets up pipeline state and screen projection.
    void Begin(ID3D12GraphicsCommandList* cmdList, ConstantBufferAllocator& cbAlloc,
      UINT screenWidth, UINT screenHeight);

    // Draw a solid-color quad (no texture).
    void XM_CALLCONV DrawRect(const RECT& dest, FXMVECTOR color);

    // End the batch and flush queued draws.
    void End();

  private:
    struct SpriteVertex
    {
      XMFLOAT2 Position;
      XMFLOAT4 Color;
    };

    void FlushBatch();

    com_ptr<ID3D12PipelineState> m_pso;
    com_ptr<ID3D12RootSignature> m_rootSignature;

    ID3D12GraphicsCommandList* m_cmdList = nullptr;
    ConstantBufferAllocator* m_cbAlloc = nullptr;

    std::vector<SpriteVertex> m_vertices;
    static constexpr uint32_t MAX_SPRITES_PER_BATCH = 512;
    static constexpr uint32_t VERTICES_PER_SPRITE = 6; // 2 triangles
  };
}
