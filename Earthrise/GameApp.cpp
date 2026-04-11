#include "pch.h"
#include "GameApp.h"

GameApp* g_app = nullptr;

namespace
{
  const std::vector<std::wstring> g_supportedLanguages =
  {
 L"ENG", L"FRA", L"ITA", L"DEU", L"ESP", L"RUS", L"POL", L"POR", L"JPN", L"KOR", L"CHS", L"CHT"
  };

  // Test scene — one mesh per category with distinct Darwinia-style neon colors
  struct TestObject
  {
    const char* meshKey;
    XMFLOAT3 position;
    XMFLOAT4 color;
    float scale;
  };

  constexpr TestObject c_testObjects[] =
  {
    { "Asteroids/Asteroid01",             {  0,  0,  10 }, { 0.8f, 0.6f, 0.2f, 1 }, 1.0f },  // Warm orange
    { "Crates/Crate01",                   {  5,  1,   8 }, { 0.0f, 1.0f, 0.5f, 1 }, 1.0f },  // Neon green
    { "Hulls/HullShuttle",                { -4,  0,  12 }, { 0.0f, 0.8f, 1.0f, 1 }, 1.0f },  // Cyan
    { "Jumpgates/Jumpgate01",             { 10,  0,  18 }, { 1.0f, 0.0f, 1.0f, 1 }, 1.0f },  // Magenta
    { "Projectiles/MissileLight",         {  3,  2,   9 }, { 1.0f, 1.0f, 0.0f, 1 }, 1.0f },  // Yellow
    { "SpaceObjects/DebrisGenericBarrel01",{ -6, -1,  11 }, { 0.5f, 0.5f, 0.5f, 1 }, 1.0f },  // Grey
    { "Stations/Mining01",                { 14,  3,  20 }, { 1.0f, 0.5f, 0.0f, 1 }, 1.0f },  // Orange
    { "Turrets/BallisticTurret01",        {  2, -2,   7 }, { 0.3f, 0.7f, 1.0f, 1 }, 1.0f },  // Light blue
  };
}

GameApp::GameApp()
{
  g_app = this;

  // Determine default language if possible
  auto isoLang = Windows::System::UserProfile::GlobalizationPreferences::Languages().GetAt(0);
  auto lang = std::wstring(Windows::Globalization::Language(isoLang).AbbreviatedName());
  auto country = Windows::Globalization::GeographicRegion().DisplayName();
  m_isoLang = to_string(isoLang);
}

GameApp::~GameApp()
{
}

Windows::Foundation::IAsyncAction GameApp::Startup()
{
  using namespace Neuron::Graphics;

  const UINT backBufferCount = Core::GetBackBufferCount();

  // GPU infrastructure — 4 MB per frame for constant buffers and dynamic vertex data
  constexpr size_t UPLOAD_HEAP_PER_FRAME = 4 * 1024 * 1024;
  m_uploadHeap.Create(UPLOAD_HEAP_PER_FRAME, backBufferCount);
  m_cbAlloc.Initialize(&m_uploadHeap);

  // Shader-visible descriptor heap — 256 descriptors per frame for SRV bindings
  m_srvHeap.Create(256, backBufferCount);

  // Camera
  auto outputSize = Core::GetOutputSize();
  float width = static_cast<float>(outputSize.right - outputSize.left);
  float height = static_cast<float>(outputSize.bottom - outputSize.top);
  m_camera.SetPerspective(XM_PIDIV4, width / height, 0.1f, 20000.0f);
  m_camera.SetPosition(XMVectorSet(0, 2, 0, 0));
  m_camera.SetLookDirection(Math::Vector3::FORWARD);
  m_camera.SetMode(CameraMode::FreeFly);

  // Renderers
  m_objectRenderer.Initialize();
  m_starfield.Initialize();
  m_postProcess.Initialize(static_cast<UINT>(width), static_cast<UINT>(height));
  m_tacticalGrid.Initialize();
  m_spriteBatch.Initialize();

  // Preload test meshes
  for (const auto& obj : c_testObjects)
    m_meshCache.GetMesh(obj.meshKey);

  m_initialized = true;
  co_return;
}

void GameApp::Shutdown()
{
  Graphics::Core::WaitForGpu();
  m_uploadHeap.Destroy();
  m_srvHeap.Destroy();
  m_initialized = false;
}

void GameApp::Update(float _deltaT)
{
  if (!m_initialized) return;
  m_camera.Update(_deltaT);
}

void GameApp::RenderScene()
{
  using namespace Neuron::Graphics;

  if (!m_initialized) return;

  auto cmdList = Core::GetCommandList();
  const UINT frameIndex = Core::GetCurrentFrameIndex();

  // Begin frame — reset per-frame allocators
  m_uploadHeap.BeginFrame(frameIndex);
  m_srvHeap.BeginFrame(frameIndex);

  // Clear render target and depth
  auto rtv = Core::GetRenderTargetView();
  auto dsv = Core::GetDepthStencilView();
  constexpr float clearColor[] = { 0.0f, 0.0f, 0.02f, 1.0f }; // Near-black space
  cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
  cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr); // Reverse-Z: clear to 0

  cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

  auto viewport = Core::GetScreenViewport();
  auto scissor = Core::GetScissorRect();
  cmdList->RSSetViewports(1, &viewport);
  cmdList->RSSetScissorRects(1, &scissor);

  // Starfield (render first, no depth write)
  m_starfield.Render(cmdList, m_cbAlloc, m_camera);

  // Space objects
  m_objectRenderer.BeginFrame(cmdList, m_cbAlloc, m_camera);

  for (const auto& obj : c_testObjects)
  {
    const Mesh* mesh = m_meshCache.GetMesh(obj.meshKey);
    if (!mesh) continue;

    XMMATRIX world = XMMatrixScaling(obj.scale, obj.scale, obj.scale) *
      XMMatrixTranslation(obj.position.x, obj.position.y, obj.position.z);

    // Origin-rebasing: subtract camera world position
    XMVECTOR camPos = m_camera.GetPosition();
    XMFLOAT3 cam;
    XMStoreFloat3(&cam, camPos);
    world.r[3] = XMVectorSubtract(world.r[3], XMVectorSet(cam.x, cam.y, cam.z, 0));

    XMVECTOR color = XMLoadFloat4(&obj.color);
    m_objectRenderer.RenderObject(cmdList, m_cbAlloc, mesh, world, color);
  }

  // Tactical grid (off by default, toggle with code)
  m_tacticalGrid.Render(cmdList, m_cbAlloc, m_camera);
}
