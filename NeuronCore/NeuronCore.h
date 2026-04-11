#pragma once

// NeuronCore.h — Full NeuronCore header with WinRT projections.
// Client projects include this. Server projects should include NeuronCoreBase.h instead.

#include "NeuronCoreBase.h"

// PROFILER_ENABLED is now set via CMake (EARTHRISE_USE_PIX option)

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

namespace Neuron
{
    class CoreEngine
    {
    public:
        static void Startup();
        static void Shutdown();
    };

}

#pragma comment(lib, "OneCore.lib")
