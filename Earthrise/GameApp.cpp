#include "pch.h"
#include "GameApp.h"
#include "Interpolation.h"
#include "GameEvents.h"

GameApp* g_app = nullptr;

namespace
{
  const std::vector<std::wstring> g_supportedLanguages =
  {
 L"ENG", L"FRA", L"ITA", L"DEU", L"ESP", L"RUS", L"POL", L"POR", L"JPN", L"KOR", L"CHS", L"CHT"
  };

  // Known mesh keys for hash registration (populated during preload).
  struct MeshKeyEntry
  {
    const char* folder;
    const char* name;
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
  using namespace Neuron;

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
  m_camera.SetPosition(XMVectorSet(0, 4, 28, 0));
  m_camera.SetLookDirection(Math::Vector3::FORWARD);
  m_camera.SetMode(CameraMode::FreeFly);

  // Renderers
  m_objectRenderer.Initialize();
  m_surfaceRenderer.Initialize();
  m_starfield.Initialize();
  m_postProcess.Initialize(static_cast<UINT>(width), static_cast<UINT>(height));
  m_tacticalGrid.Initialize();
  m_spriteBatch.Initialize();

  // Preload all mesh categories for fast lookup
  for (auto cat = SpaceObjectCategory::SpaceObject; cat < SpaceObjectCategory::COUNT; ++cat)
  {
    m_meshCache.PreloadCategory(cat);
  }

  // Log the bounds of the first asteroid mesh for scale diagnostics
  if (auto* mesh = m_meshCache.GetMesh("Asteroids/Asteroid01"))
  {
    DebugTrace("MeshCache: Asteroid01 loaded — verts={}, indices={}, submeshes={}\n",
               mesh->GetVertexCount(), mesh->GetIndexCount(),
               mesh->GetSubmeshes().size());
  }
  else
  {
    DebugTrace("MeshCache: Asteroid01 NOT FOUND after preload!\n");
  }

  // Build mesh hash reverse lookup: server sends bare-name hashes (e.g., HashMeshName("Asteroid01")),
  // but MeshCache keys are "Category/BareName" (e.g., "Asteroids/Asteroid01"). Register both.
  {
    std::unordered_map<uint32_t, std::string> hashMap;
    for (auto cat = SpaceObjectCategory::SpaceObject; cat < SpaceObjectCategory::COUNT; ++cat)
    {
      m_meshCache.BuildMeshHashMap(cat, hashMap);
    }
    for (const auto& [hash, key] : hashMap)
    {
      m_worldState.RegisterMeshName(hash, key);
    }
  }

  // Input setup
  m_inputState.SetScreenSize(width, height);
  m_commandTargeting.Initialize(&m_worldState, &m_camera);
  m_commandTargeting.SetScreenSize(width, height);
  m_inputHandler.Initialize(&m_serverConnection, &m_worldState, &m_camera,
    &m_commandTargeting, &m_targetingUI, &m_chatUI);
  m_fleetSelectionUI.Initialize(&m_worldState, &m_camera);

  // HUD and UI setup
  m_hud.Initialize(&m_worldState);
  m_targetingUI.Initialize(&m_worldState, &m_camera);
  m_fleetPanel.Initialize(&m_worldState);
  m_jumpgateUI.Initialize(&m_worldState, &m_camera);

  // Audio & Effects (Phase 9)
  m_spatialAudio.Initialize(nullptr); // No audio hardware yet; silent mode
  m_spatialAudio.PreloadFromBank(m_soundBank);
  m_particleRenderer.Initialize();
  m_audioEventHandler.Initialize(&m_spatialAudio, &m_particleSystem);

  // Connect to the local server.
  m_serverConnection.Connect("127.0.0.1", 7777, "Player1");

  m_initialized = true;
  co_return;
}

void GameApp::Shutdown()
{
  m_serverConnection.Disconnect();
  Graphics::Core::WaitForGpu();
  m_uploadHeap.Destroy();
  m_srvHeap.Destroy();
  m_initialized = false;
}

bool GameApp::ProcessInput(UINT _message, WPARAM _wParam, LPARAM _lParam)
{
  return m_inputState.ProcessMessage(_message, _wParam, _lParam);
}

void GameApp::EndInputFrame()
{
  m_inputState.EndFrame();
}

void GameApp::Update(float _deltaT)
{
  if (!m_initialized) return;

  // Begin input frame
  m_inputState.BeginFrame();

  // Process network messages
  ProcessServerMessages();

  // Send heartbeat to keep server session alive (~10 Hz)
  m_heartbeatTimer += _deltaT;
  if (m_heartbeatTimer >= 10.0f)
  {
    m_serverConnection.Send(Neuron::MessageId::Heartbeat, nullptr, 0);
    m_heartbeatTimer = 0.0f;
  }

  // Update world state (interpolation)
  m_worldState.Update(_deltaT);

  // Update prediction for owned fleet
  m_prediction.Update(_deltaT, 100.0f); // default move speed for prediction

  // Handle input → camera, fleet commands
  m_inputHandler.Update(_deltaT, m_inputState);

  // Update fleet selection UI (box-drag state)
  m_fleetSelectionUI.Update(_deltaT, m_inputState);
  m_fleetSelectionUI.SetSelection(m_inputHandler.GetSelectedShips());

  // Update targeting and HUD
  m_targetingUI.RefreshTargetList();
  m_hud.SetFocusEntity(m_worldState.GetFlagship());
  m_hud.SetTarget(m_targetingUI.GetTarget());

  // Update fleet panel with selected ships
  m_fleetPanel.SetFleet(m_inputHandler.GetSelectedShips());

  // Update jumpgate proximity
  m_jumpgateUI.Update(m_worldState.GetFlagship());

  // Update chat message ages
  m_chatUI.Update(_deltaT);

  // Update particles (Phase 9)
  m_particleSystem.Update(_deltaT);

  // Update spatial audio listener from camera (Phase 9)
  m_spatialAudio.UpdateListener(m_camera.GetPosition(),
    m_camera.GetForward(), m_camera.GetUp(), _deltaT);

  // Camera update
  m_camera.Update(_deltaT);
}

void GameApp::ProcessServerMessages()
{
  using namespace Neuron;

  std::vector<ReceivedMessage> messages;
  m_serverConnection.DrainMessages(messages, 64);

  for (const auto& msg : messages)
  {
    DataReader reader(msg.Payload.data(), static_cast<int>(msg.Payload.size()));

    switch (msg.MsgId)
    {
    case MessageId::EntitySpawn:
    {
      EntitySpawnMsg spawn;
      spawn.Read(reader);
      m_worldState.ApplySpawn(spawn);
      {
        const auto* ent = m_worldState.GetEntity(spawn.Handle);
        DebugTrace("GameApp: Entity spawned (handle={}, cat={}, mesh={}, key=\"{}\", pos=({:.0f},{:.0f},{:.0f})). Active: {}\n",
                   spawn.Handle.m_id, static_cast<int>(spawn.Category),
                   spawn.MeshHash, ent ? ent->MeshKey : "<null>",
                   spawn.Position.x, spawn.Position.y, spawn.Position.z,
                   m_worldState.ActiveCount());
      }
      break;
    }

    case MessageId::EntityDespawn:
    {
      EntityDespawnMsg despawn;
      despawn.Read(reader);

      // Publish explosion event before removing the entity (Phase 9).
      if (const auto* ent = m_worldState.GetEntity(despawn.Handle))
      {
        Neuron::ExplosionEvent evt;
        evt.Entity   = despawn.Handle;
        evt.Position = ent->Position;
        evt.Radius   = 2.0f;
        EventManager::Publish(evt);
      }

      m_worldState.ApplyDespawn(despawn);
      m_prediction.Remove(despawn.Handle);
      break;
    }

    case MessageId::StateSnapshot:
    {
      StateSnapshotMsg snapshot;
      snapshot.Read(reader);
      m_worldState.ApplySnapshot(snapshot);

      // Reconcile predictions with server state
      for (const auto& state : snapshot.Entities)
      {
        EntityHandle h;
        h.m_id = state.HandleId;
        m_prediction.Reconcile(h, state.Position);
      }
      break;
    }

    case MessageId::ChatMessage:
    {
      ChatMsg chat;
      chat.Read(reader);
      DebugTrace("[Chat] {}: {}\n", chat.SenderName, chat.Text);
      m_chatUI.AddMessage(chat.Channel, chat.SenderName, chat.Text);
      break;
    }

    case MessageId::ShipStatus:
    {
      ShipStatusMsg status;
      status.Read(reader);
      m_worldState.ApplyShipStatus(status);
      break;
    }

    case MessageId::PlayerInfo:
    {
      PlayerInfoMsg info;
      info.Read(reader);
      m_worldState.SetFlagship(info.FlagshipHandle);
      m_hud.SetFocusEntity(info.FlagshipHandle);
      break;
    }

    default:
      break;
    }
  }
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

  // Render world entities
  RenderWorldEntities(cmdList);

  // Render particles (Phase 9)
  m_particleRenderer.Render(cmdList, m_cbAlloc, m_camera, m_particleSystem);

  // Tactical grid
  m_tacticalGrid.Render(cmdList, m_cbAlloc, m_camera);
}

void GameApp::RenderWorldEntities(ID3D12GraphicsCommandList* _cmdList)
{
  using namespace Neuron::Graphics;
  using Neuron::SpaceObjectCategory;

  int totalCount = 0, noKeyCount = 0, noMeshCount = 0, renderedCount = 0;
  float nearestDist = 99999.0f;
  XMVECTOR camPos = m_camera.GetPosition();

  // Pass 1: Non-asteroid entities (flat-color pipeline)
  m_objectRenderer.BeginFrame(_cmdList, m_cbAlloc, m_camera);
  m_worldState.ForEachActive([&](const ClientEntity& entity)
  {
    if (entity.Category == SpaceObjectCategory::Asteroid)
      return;

    ++totalCount;
    std::string meshKey = entity.MeshKey;
    if (meshKey.empty()) { ++noKeyCount; return; }

    const Mesh* mesh = m_meshCache.GetMesh(meshKey);
    if (!mesh) { ++noMeshCount; return; }

    XMFLOAT3 renderPos = entity.Position;
    const XMFLOAT3* predicted = m_prediction.GetPredictedPosition(entity.Handle);
    if (predicted) renderPos = *predicted;

    XMVECTOR entPos = XMLoadFloat3(&renderPos);
    float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(entPos, camPos)));
    if (dist < nearestDist) nearestDist = dist;

    XMVECTOR orientation = XMLoadFloat4(&entity.Orientation);
    XMMATRIX rotMatrix = XMMatrixRotationQuaternion(orientation);
    XMMATRIX world = rotMatrix *
      XMMatrixTranslation(renderPos.x, renderPos.y, renderPos.z);
    world.r[3] = XMVectorSubtract(world.r[3], XMVectorSetW(camPos, 0));

    XMVECTOR color = XMLoadFloat4(&entity.Color);
    m_objectRenderer.RenderObject(_cmdList, m_cbAlloc, mesh, world, color);
    ++renderedCount;
  });

  // Pass 2: Asteroid entities (surface-colored pipeline)
  m_surfaceRenderer.BeginFrame(_cmdList, m_cbAlloc, m_camera);
  m_worldState.ForEachActive([&](const ClientEntity& entity)
  {
    if (entity.Category != SpaceObjectCategory::Asteroid)
      return;

    ++totalCount;
    std::string meshKey = entity.MeshKey;
    if (meshKey.empty()) { ++noKeyCount; return; }

    XMFLOAT3 renderPos = entity.Position;
    const XMFLOAT3* predicted = m_prediction.GetPredictedPosition(entity.Handle);
    if (predicted) renderPos = *predicted;

    XMVECTOR entPos = XMLoadFloat3(&renderPos);
    float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(entPos, camPos)));
    if (dist < nearestDist) nearestDist = dist;

    XMVECTOR orientation = XMLoadFloat4(&entity.Orientation);
    XMMATRIX rotMatrix = XMMatrixRotationQuaternion(orientation);
    XMMATRIX world = rotMatrix *
      XMMatrixTranslation(renderPos.x, renderPos.y, renderPos.z);
    world.r[3] = XMVectorSubtract(world.r[3], XMVectorSetW(camPos, 0));

    SurfaceMesh* surfMesh = m_surfaceRenderer.GetSurfaceMesh(meshKey, entity.SurfaceType);
    if (!surfMesh) { ++noMeshCount; return; }

    m_surfaceRenderer.RenderObject(_cmdList, m_cbAlloc, surfMesh, world);
    ++renderedCount;
  });

#if defined(_DEBUG)
  // Show render stats in window title for diagnostics
  {
    XMFLOAT3 cp;
    XMStoreFloat3(&cp, camPos);
    char buf[512];
    sprintf_s(buf, "DSO — ent:%d key:%d mesh:%d draw:%d near:%.0f cam:(%.0f,%.0f,%.0f) meshes:%zu",
              totalCount, noKeyCount, noMeshCount, renderedCount,
              nearestDist, cp.x, cp.y, cp.z, m_meshCache.GetCacheSize());
    SetWindowTextA(ClientEngine::Window(), buf);
  }
#endif
}

void GameApp::RenderCanvas()
{
  using namespace Neuron::Graphics;

  if (!m_initialized) return;

  auto cmdList = Core::GetCommandList();
  auto outputSize = Core::GetOutputSize();
  UINT screenW = static_cast<UINT>(outputSize.right - outputSize.left);
  UINT screenH = static_cast<UINT>(outputSize.bottom - outputSize.top);

  // Fleet selection UI (brackets and drag box)
  m_fleetSelectionUI.Render(cmdList, m_cbAlloc, m_spriteBatch, screenW, screenH);

  // Target bracket
  m_targetingUI.Render(cmdList, m_cbAlloc, m_spriteBatch, screenW, screenH);

  // HUD overlay (player bars, target panel, crosshair)
  m_hud.Render(cmdList, m_cbAlloc, m_spriteBatch, screenW, screenH);

  // Fleet command panel
  m_fleetPanel.Render(cmdList, m_cbAlloc, m_spriteBatch, screenW, screenH);

  // Chat
  m_chatUI.Render(cmdList, m_cbAlloc, m_spriteBatch, screenW, screenH);

  // Jumpgate warp indicator
  m_jumpgateUI.Render(cmdList, m_cbAlloc, m_spriteBatch, screenW, screenH);
}
