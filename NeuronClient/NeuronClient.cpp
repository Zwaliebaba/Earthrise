#include "pch.h"
#include "NeuronClient.h"
#include "EventManager.h"

namespace
{
  float windowWidthAdd, windowHeightAdd;
  constexpr bool SHOW_BORDER = true;
}

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

void ClientEngine::Startup(const wchar_t* _gameName, Windows::Foundation::Size _size, HINSTANCE _hInstance, int nCmdShow)
{
  CoreEngine::Startup();

  m_instance = _hInstance;
  m_outputSize = _size;

  // Register class
  WNDCLASSEX wcex = {};
  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  wcex.lpfnWndProc = WndProc;
  wcex.hInstance = _hInstance;
  wcex.lpszClassName = L"Neuron";
  wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  ASSERT(RegisterClassEx(&wcex));

  Graphics::Core::Startup();

  // Create window
  DWORD style;

  if constexpr (SHOW_BORDER)
  {
    style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_EX_TOPMOST; 
    windowWidthAdd = GetSystemMetrics(SM_CXSIZEFRAME) * 2.0f;
    windowHeightAdd = GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYSIZEFRAME) * 2.0f;
  }
  else
  {
    style = WS_POPUP;
    windowWidthAdd = 0;
    windowHeightAdd = 0;
  }

  const int windowWidth = _size.Width + windowWidthAdd;
  const int windowHeight = _size.Height + windowHeightAdd;

  m_hwnd = CreateWindowEx(0, L"Neuron", _gameName, style, CW_USEDEFAULT, CW_USEDEFAULT, windowWidth,
    windowHeight, nullptr, nullptr, m_instance, nullptr);

  if (!m_hwnd)
  {
    LPTSTR lpMsgBuf;
    DWORD dw = GetLastError();

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
      dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, nullptr);

    Fatal(L"CreateWindowEx failed with error {:d}: {:s}", dw, lpMsgBuf);
  }

  RECT rc;
  GetClientRect(m_hwnd, &rc);

  Graphics::Core::SetWindow(m_hwnd, rc.right - rc.left, rc.bottom - rc.top);

  ShowWindow(m_hwnd, nCmdShow);
  ShowCursor(FALSE);
}

void ClientEngine::StartGame(const com_ptr<GameMain>&& _gameMain)
{
  m_main = _gameMain;
  SetWindowLongPtr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(m_main.get()));
  m_main->Startup();
}

void ClientEngine::Shutdown()
{
  if (m_main)
  {
    m_main->Shutdown();
    m_main = nullptr;
  }
  
  Graphics::Core::Shutdown();

  CoreEngine::Shutdown();
}

Windows::Foundation::Point ClientEngine::OutputTopLeft()
{
  RECT rc;
  GetWindowRect(m_hwnd, &rc);

  // This is the top left of the window, not the client area, dont know why I have to add 3 (or 1 on my X64)
  return { rc.left + windowWidthAdd, rc.top + windowHeightAdd };
}

LRESULT CALLBACK WndProc(const HWND _hWnd, const UINT _message, WPARAM _wParam, const LPARAM _lParam)
{
  static bool s_in_sizemove = false;
  static bool s_in_suspend = false;
  static bool s_minimized = false;
  static bool s_fullscreen = false;

  auto* game = reinterpret_cast<GameMain*>(GetWindowLongPtr(_hWnd, GWLP_USERDATA));

  switch (_message)
  {
  case WM_CREATE:
    if (_lParam)
    {
      const auto params = reinterpret_cast<LPCREATESTRUCTW>(_lParam);
      SetWindowLongPtr(_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(params->lpCreateParams));
    }
    break;

  case WM_CLOSE:
    DestroyWindow(_hWnd);
    break;

  case WM_PAINT:
  {
    PAINTSTRUCT ps;
    BeginPaint(_hWnd, &ps);
    EndPaint(_hWnd, &ps);
    return 0;
  }

  case WM_DISPLAYCHANGE:
    if (game)
      game->OnDisplayChange();
    break;

  case WM_MOVE:
    if (game)
      game->OnWindowMoved();
    break;

  case WM_SIZE:
    if (_wParam == SIZE_MINIMIZED)
    {
      if (!s_minimized)
      {
        s_minimized = true;
        if (!s_in_suspend && game)
          game->OnSuspending();
        s_in_suspend = true;
      }
    }
    else if (s_minimized)
    {
      s_minimized = false;
      if (s_in_suspend && game)
        game->OnResuming();
      s_in_suspend = false;
    }
    else if (!s_in_sizemove && game)
      game->OnWindowSizeChanged(LOWORD(_lParam), HIWORD(_lParam));
    break;

  case WM_ENTERSIZEMOVE:
    s_in_sizemove = true;
    break;

  case WM_EXITSIZEMOVE:
    s_in_sizemove = false;
    if (game)
    {
      RECT rc;
      GetClientRect(_hWnd, &rc);

      game->OnWindowSizeChanged(rc.right - rc.left, rc.bottom - rc.top);
    }
    break;

  case WM_GETMINMAXINFO:
    if (_lParam)
    {
      auto* info = reinterpret_cast<MINMAXINFO*>(_lParam);
      info->ptMinTrackSize.x = 320;
      info->ptMinTrackSize.y = 200;
    }
    break;

  case WM_ACTIVATEAPP:
    if (game)
    {
      if (_wParam)
        game->OnActivated();
      else
        game->OnDeactivated();
    }
    break;

  case WM_POWERBROADCAST:
    switch (_wParam)
    {
    case PBT_APMQUERYSUSPEND:
      if (!s_in_suspend && game)
        game->OnSuspending();
      s_in_suspend = true;
      return TRUE;

    case PBT_APMRESUMESUSPEND:
      if (!s_minimized)
      {
        if (s_in_suspend && game)
          game->OnResuming();
        s_in_suspend = false;
      }
      return TRUE;
    }
    break;

  case WM_DESTROY:
    PostQuitMessage(0);
    break;

  case WM_SYSKEYDOWN:
    if (_wParam == VK_RETURN && (_lParam & 0x60000000) == 0x20000000)
    {
      // Implements the classic ALT+ENTER fullscreen toggle
      if (s_fullscreen)
      {
        SetWindowLongPtr(_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
        SetWindowLongPtr(_hWnd, GWL_EXSTYLE, 0);

        ShowWindow(_hWnd, SW_SHOWNORMAL);

        const auto outputSize = ClientEngine::OutputSize();
        SetWindowPos(_hWnd, HWND_TOP, 0, 0, outputSize.Width, outputSize.Height,
          SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
      }
      else
      {
        SetWindowLongPtr(_hWnd, GWL_STYLE, WS_POPUP);
        SetWindowLongPtr(_hWnd, GWL_EXSTYLE, WS_EX_TOPMOST);

        SetWindowPos(_hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

        ShowWindow(_hWnd, SW_SHOWMAXIMIZED);
      }

      if (game)
      {
        RECT rc;
        GetClientRect(_hWnd, &rc);

        game->OnWindowSizeChanged(rc.right - rc.left, rc.bottom - rc.top);
      }

      s_fullscreen = !s_fullscreen;
    }
    break;

  case WM_MENUCHAR:
    // A menu is active and the user presses a key that does not correspond
    // to any mnemonic or accelerator key. Ignore so we don't produce an error beep.
    return MAKELRESULT(0, MNC_CLOSE);
  }

  if (EventManager::WndProc(_hWnd, _message, _wParam, _lParam) == -1)
    return DefWindowProc(_hWnd, _message, _wParam, _lParam);

  return 0;
}

void ClientEngine::OnDeviceLost() {}

void ClientEngine::OnDeviceRestored() {}
