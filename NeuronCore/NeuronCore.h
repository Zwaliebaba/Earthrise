#pragma once

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#if defined (_DEBUG) 
#define PROFILER_ENABLED
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <concurrent_queue.h>
#include <concurrent_unordered_map.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <exception>
#include <format>
#include <functional>
#include <future>
#include <iterator>
#include <map>
#include <mdspan>
#include <memory>
#include <queue>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Use the C++ standard templated min/max
#define NOMINMAX

// DirectX apps don't need GDI
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP

// Include <mcx.h> if you need this
#define NOMCX

// Include <winsvc.h> if you need this
#define NOSERVICE

// WinHelp is deprecated
#define NOHELP

#if !defined WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include <Windows.h>
#include <hstring.h>
#include <restrictederrorinfo.h>
#include <unknwn.h>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.System.Threading.Core.h>
#include <winrt/Windows.System.UserProfile.h>
#include <winrt/Windows.System.h>

using namespace winrt;

#include "Debug.h"
#include "GameMath.h"

#include "NeuronHelper.h"

using namespace Neuron;

#include "FileSys.h"
#include "TimerCore.h"

namespace Neuron
{
    constexpr uint32_t ENGINE_VERSION = 0x00000001;

    class CoreEngine
    {
    public:
        static void Startup();
        static void Shutdown();
    };

}
