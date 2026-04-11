#include "pch.h"
#include "ServerLoop.h"

int main()
{
    Neuron::DebugTrace("EarthRiseServer starting...\n");

    Neuron::InitializeNetworking();

    EarthRise::ServerLoop server;
    if (!server.Startup())
    {
        Neuron::DebugTrace("EarthRiseServer: Startup failed\n");
        Neuron::ShutdownNetworking();
        return 1;
    }

    server.Run();
    server.Shutdown();

    Neuron::ShutdownNetworking();

    Neuron::DebugTrace("EarthRiseServer exited\n");
    return 0;
}