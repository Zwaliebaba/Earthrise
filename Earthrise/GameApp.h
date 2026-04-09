#pragma once

#include "GameMain.h"

class GameApp : public GameMain
{
  public:
    GameApp();
    ~GameApp() override;

    Windows::Foundation::IAsyncAction Startup() override { co_return; }
    void Shutdown() override {}
    void Update([[maybe_unused]] float _deltaT) override {}
    void RenderScene() override {}

  protected:
    std::string m_isoLang;
};

extern GameApp* g_app;
