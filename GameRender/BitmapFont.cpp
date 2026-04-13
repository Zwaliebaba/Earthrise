#include "pch.h"
#include "BitmapFont.h"
#include "DdsLoader.h"
#include "GpuResourceManager.h"
#include "CompiledShaders/FontVS.h"
#include "CompiledShaders/FontPS.h"

using namespace Neuron::Graphics;

// Static pipeline shared by all BitmapFont instances
com_ptr<ID3D12PipelineState>  BitmapFont::s_pso;
com_ptr<ID3D12RootSignature>  BitmapFont::s_rootSignature;
bool BitmapFont::s_pipelineReady = false;

void BitmapFont::CreatePipeline()
{
  if (s_pipelineReady)
    return;

  // Root signature: b0 = CBV (projection), t0 = SRV descriptor table (font texture), s0 = static sampler
  CD3DX12_ROOT_PARAMETER rootParams[2];
  rootParams[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

  CD3DX12_DESCRIPTOR_RANGE srvRange;
  srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  rootParams[1].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

  // Point sampler for crisp pixel-art glyphs
  D3D12_STATIC_SAMPLER_DESC sampler{};
  sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
  sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.ShaderRegister = 0;
  sampler.RegisterSpace = 0;
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  sampler.MaxLOD = D3D12_FLOAT32_MAX;

  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
  rootSigDesc.Init(_countof(rootParams), rootParams, 1, &sampler,
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  s_rootSignature = PipelineHelpers::CreateRootSignature(rootSigDesc);
  SetName(s_rootSignature.get(), L"BitmapFontRootSig");

  D3D12_INPUT_ELEMENT_DESC inputLayout[] =
  {
    { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0,                            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, sizeof(XMFLOAT2),             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, sizeof(XMFLOAT2) * 2,        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
  psoDesc.pRootSignature = s_rootSignature.get();
  psoDesc.VS = { g_pFontVS, sizeof(g_pFontVS) };
  psoDesc.PS = { g_pFontPS, sizeof(g_pFontPS) };
  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

  // Alpha blending for text
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
  psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
  psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
  psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  psoDesc.DepthStencilState.DepthEnable = FALSE;
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = Core::GetBackBufferFormat();
  psoDesc.SampleDesc.Count = 1;

  s_pso = PipelineHelpers::CreateGraphicsPSO(psoDesc);
  SetName(s_pso.get(), L"BitmapFontPSO");

  s_pipelineReady = true;
}

void BitmapFont::LoadFromFile(const std::wstring& relativePath)
{
  CreatePipeline();

  std::wstring fullPath = FileSys::GetHomeDirectory();
  fullPath += relativePath;

  CpuImage image = DdsLoader::LoadFromFile(fullPath);
  if (image.Pixels.empty())
  {
    Neuron::DebugTrace("BitmapFont: failed to load {}\n", std::string(relativePath.begin(), relativePath.end()));
    return;
  }

  m_texWidth = image.Width;
  m_texHeight = image.Height;

  m_texture = GpuResourceManager::CreateStaticTexture(image, L"BitmapFontTexture");

  // Create non-shader-visible SRV (copied to shader-visible heap at draw time)
  m_srvCPU = Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Texture2D.MipLevels = 1;
  Core::GetD3DDevice()->CreateShaderResourceView(m_texture.get(), &srvDesc, m_srvCPU);

  // Set these last so IsLoaded() only returns true when the texture is ready
  m_pso = s_pso.get();
  m_rootSignature = s_rootSignature.get();

  m_vertices.reserve(MAX_GLYPHS_PER_BATCH * VERTICES_PER_GLYPH);
}

void BitmapFont::BeginDraw(ID3D12GraphicsCommandList* cmdList,
  ConstantBufferAllocator& cbAlloc,
  ShaderVisibleHeap& srvHeap,
  UINT screenWidth, UINT screenHeight)
{
  if (!m_pso) return;

  m_cmdList = cmdList;
  m_cbAlloc = &cbAlloc;
  m_srvHeap = &srvHeap;
  m_vertices.clear();

  // Orthographic projection (screen space: origin top-left)
  XMMATRIX proj = XMMatrixOrthographicOffCenterLH(
    0, static_cast<float>(screenWidth),
    static_cast<float>(screenHeight), 0,
    0, 1);

  struct { XMFLOAT4X4 Projection; } data;
  XMStoreFloat4x4(&data.Projection, XMMatrixTranspose(proj));

  auto alloc = cbAlloc.Allocate(sizeof(data));
  memcpy(alloc.CpuAddress, &data, sizeof(data));

  // Copy SRV to shader-visible heap
  auto srvHandle = srvHeap.Allocate(1);
  Core::GetD3DDevice()->CopyDescriptorsSimple(1,
    static_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(srvHandle),
    m_srvCPU,
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  // Bind pipeline
  ID3D12DescriptorHeap* heaps[] = { srvHeap.GetHeap() };
  cmdList->SetDescriptorHeaps(1, heaps);

  cmdList->SetPipelineState(m_pso);
  cmdList->SetGraphicsRootSignature(m_rootSignature);
  cmdList->SetGraphicsRootConstantBufferView(0, alloc.GpuAddress);
  cmdList->SetGraphicsRootDescriptorTable(1, srvHandle);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void XM_CALLCONV BitmapFont::DrawString(float x, float y, std::string_view text,
  FXMVECTOR color, float scale)
{
  if (!m_cmdList || m_texWidth == 0) return;

  XMFLOAT4 col;
  XMStoreFloat4(&col, color);

  const float glyphW = static_cast<float>(GLYPH_WIDTH) * scale;
  const float glyphH = static_cast<float>(GLYPH_HEIGHT) * scale;
  const float uStep = static_cast<float>(GLYPH_WIDTH) / static_cast<float>(m_texWidth);
  const float vStep = static_cast<float>(GLYPH_HEIGHT) / static_cast<float>(m_texHeight);

  float cursorX = x;

  for (char ch : text)
  {
    auto c = static_cast<uint8_t>(ch);
    if (c < FIRST_CHAR)
    {
      cursorX += glyphW;
      continue;
    }

    uint32_t glyphIndex = c - FIRST_CHAR;
    if (glyphIndex >= COLUMNS * ROWS)
    {
      cursorX += glyphW;
      continue;
    }

    uint32_t col_idx = glyphIndex % COLUMNS;
    uint32_t row_idx = glyphIndex / COLUMNS;

    float u0 = static_cast<float>(col_idx) * uStep;
    float v0 = static_cast<float>(row_idx) * vStep;
    float u1 = u0 + uStep;
    float v1 = v0 + vStep;

    float left   = cursorX;
    float top    = y;
    float right  = cursorX + glyphW;
    float bottom = y + glyphH;

    // Two triangles (6 vertices)
    m_vertices.push_back({ { left,  top },    { u0, v0 }, col });
    m_vertices.push_back({ { right, top },    { u1, v0 }, col });
    m_vertices.push_back({ { left,  bottom }, { u0, v1 }, col });
    m_vertices.push_back({ { right, top },    { u1, v0 }, col });
    m_vertices.push_back({ { right, bottom }, { u1, v1 }, col });
    m_vertices.push_back({ { left,  bottom }, { u0, v1 }, col });

    if (m_vertices.size() >= MAX_GLYPHS_PER_BATCH * VERTICES_PER_GLYPH)
      FlushBatch();

    cursorX += glyphW;
  }
}

void BitmapFont::EndDraw()
{
  if (!m_vertices.empty())
    FlushBatch();
  m_cmdList = nullptr;
  m_cbAlloc = nullptr;
  m_srvHeap = nullptr;
}

void BitmapFont::FlushBatch()
{
  if (m_vertices.empty() || !m_cmdList) return;

  const size_t dataSize = m_vertices.size() * sizeof(FontVertex);
  auto alloc = m_cbAlloc->Allocate(dataSize);
  memcpy(alloc.CpuAddress, m_vertices.data(), dataSize);

  D3D12_VERTEX_BUFFER_VIEW vbView{};
  vbView.BufferLocation = alloc.GpuAddress;
  vbView.SizeInBytes = static_cast<UINT>(dataSize);
  vbView.StrideInBytes = sizeof(FontVertex);
  m_cmdList->IASetVertexBuffers(0, 1, &vbView);

  m_cmdList->DrawInstanced(static_cast<UINT>(m_vertices.size()), 1, 0, 0);
  m_vertices.clear();
}

float BitmapFont::MeasureString(std::string_view text, float scale) const noexcept
{
  return static_cast<float>(text.size()) * static_cast<float>(GLYPH_WIDTH) * scale;
}
