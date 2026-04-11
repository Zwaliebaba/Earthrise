#pragma once

#include "GameMain.h"
#include "GameRender.h"

class GameApp : public GameMain
{
  public:
    GameApp();
    ~GameApp() override;

    Windows::Foundation::IAsyncAction Startup() override;
    void Shutdown() override;
    void Update(float _deltaT) override;
    void RenderScene() override;

  protected:
    std::string m_isoLang;

  private:
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

    bool m_initialized = false;
};

extern GameApp* g_app;
