#pragma once

#include "GameMain.h"
#include "GameRender.h"
#include "ClientWorldState.h"
#include "Prediction.h"
#include "InputState.h"
#include "InputHandler.h"
#include "FleetSelectionUI.h"
#include "CommandTargeting.h"
#include "HUD.h"
#include "TargetingUI.h"
#include "ChatUI.h"
#include "FleetPanel.h"
#include "JumpgateUI.h"
#include "SoundBank.h"
#include "SpatialAudioSystem.h"
#include "AudioEventHandler.h"
#include "ParticleSystem.h"
#include "ParticleRenderer.h"
#include "ServerConnection.h"

class GameApp : public GameMain
{
  public:
    GameApp();
    ~GameApp() override;

    Windows::Foundation::IAsyncAction Startup() override;
    void Shutdown() override;
    void Update(float _deltaT) override;
    void RenderScene() override;
    void RenderCanvas() override;

    // Input forwarding from WndProc.
    bool ProcessInput(UINT _message, WPARAM _wParam, LPARAM _lParam);

    // Snapshot input state at end of frame (before next frame's messages).
    void EndInputFrame();

  protected:
    std::string m_isoLang;

  private:
    void ProcessServerMessages();
    void RenderWorldEntities(ID3D12GraphicsCommandList* _cmdList);

    // GPU infrastructure
    Neuron::Graphics::UploadHeap m_uploadHeap;
    Neuron::Graphics::ConstantBufferAllocator m_cbAlloc;
    Neuron::Graphics::ShaderVisibleHeap m_srvHeap;

    // Camera
    Neuron::Graphics::Camera m_camera;

    // Asset cache
    Neuron::Graphics::MeshCache m_meshCache;

    // Renderers
    Neuron::Graphics::SpaceObjectRenderer m_objectRenderer;
    Neuron::Graphics::Starfield m_starfield;
    Neuron::Graphics::PostProcess m_postProcess;
    Neuron::Graphics::TacticalGrid m_tacticalGrid;
    Neuron::Graphics::SpriteBatch m_spriteBatch;

    // Networking
    ServerConnection m_serverConnection;

    // World state
    ClientWorldState m_worldState;
    Prediction m_prediction;

    // Input
    InputState m_inputState;
    InputHandler m_inputHandler;

    // UI
    FleetSelectionUI m_fleetSelectionUI;
    CommandTargeting m_commandTargeting;
    HUD m_hud;
    TargetingUI m_targetingUI;
    ChatUI m_chatUI;
    FleetPanel m_fleetPanel;
    JumpgateUI m_jumpgateUI;

    // Audio & Effects (Phase 9)
    SoundBank m_soundBank;
    SpatialAudioSystem m_spatialAudio;
    AudioEventHandler m_audioEventHandler;
    Neuron::Graphics::ParticleSystem m_particleSystem;
    Neuron::Graphics::ParticleRenderer m_particleRenderer;

    bool m_initialized = false;
    bool m_connected = false;
    float m_heartbeatTimer = 0.0f;
};

extern GameApp* g_app;
