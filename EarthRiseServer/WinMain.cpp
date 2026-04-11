#include "pch.h"
#include "ServerLoop.h"

int main()
{
    Neuron::Server::ServerLog("EarthRiseServer starting...\n");

    Neuron::InitializeNetworking();

    EarthRise::ServerLoop server;
    if (!server.Startup())
    {
        Neuron::Server::ServerLog("EarthRiseServer: Startup failed\n");
        Neuron::ShutdownNetworking();
        return 1;
    }

    server.Run();
    server.Shutdown();

    Neuron::ShutdownNetworking();

    Neuron::Server::ServerLog("EarthRiseServer exited\n");
    return 0;
}