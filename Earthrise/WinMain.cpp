#include "pch.h"
#include "GameApp.h"

int WINAPI wWinMain(HINSTANCE _hInstance, HINSTANCE _hPrevInstance, LPWSTR _cmdLine, int _iCmdShow)
{
#if defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  wchar_t filename[MAX_PATH];
  GetModuleFileNameW(nullptr, filename, MAX_PATH);
  auto path = std::wstring(filename);
  path = path.substr(0, path.find_last_of('\\'));

  FileSys::SetHomeDirectory(path);

  constexpr Windows::Foundation::Size size = { 1024, 768 };

  ClientEngine::Startup(L"Deep Space Outpost", size, _hInstance, _iCmdShow);

  auto main = winrt::make_self<GameApp>();
  ClientEngine::StartGame(main);

  // Main message loop
  MSG msg = {};
  while (WM_QUIT != msg.message)
  {
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    else
    {
      auto deltaT = Timer::Core::GetElapsedSeconds();
      main->Update(deltaT);

      Graphics::Core::Prepare();
      main->RenderScene();
      Graphics::Core::Present();
    }
  }

  ClientEngine::Shutdown();
  main = nullptr;

  return WM_QUIT;
}
