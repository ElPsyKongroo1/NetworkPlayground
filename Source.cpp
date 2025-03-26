#include <iostream>
#include "stdio.h"
#include <cstdlib>
#include <string>
#include "Util/StringOperations.h"
// For input
#include <conio.h>
#include <optional>

#include "Network/Server.h"
#include "Network/Client.h"

enum class AppType
{
    Server, Client
};

static std::optional<AppType> HandleCMDArguments(int argc, char* argv[])
{
    if (argc != 2)
    {
        TSLogger::Log("Failed to start the client/server. Invalid argument count.\n");
        TSLogger::Log("    Valid command line arguments example: Server\n");
        return {};
    }

    if (strcmp(argv[1], "Server") == 0)
    {
        return AppType::Server;
    }
    else if (strcmp(argv[1], "Client") == 0)
    {
        return AppType::Client;
    }
    else
    {
        TSLogger::Log("Invalid first parameter. Please provide either \"Server\" or \"Client\"\n");
        return {};
    }

#if 0
    char* endptr;
    int m_Port = (int)std::strtol(argv[2], &endptr, 10);

    if (*endptr != '\0')
    {
        TSLogger::Log("Could not retrieve port from input as an integer. Please provide a valid integer\n");
        return {};
    }

    // Try to select a socket port 1024 (used by other services) < port < 50'000 (usually dynamically reserved)
    if (m_Port <= 1024 || m_Port >= 50'000)
    {
        TSLogger::Log("Invalid port value provided. Needs to be between 1024-50,000\n");
        return {};
    }
#endif
}

static bool OpenServer(const NetworkConfig& config)
{
    Server activeServer;

    if (!activeServer.InitServer(config))
    {
        TSLogger::Log("Failed to initialize server");
        return false;
    }

    activeServer.WaitOnServerTerminate();

    return true;
}

static bool OpenClient(const NetworkConfig& config)
{
    Client activeClient;
    LoopTimer loopTimer;
    loopTimer.InitializeTimer();

    if (!activeClient.InitClient(config))
    {
        TSLogger::Log("Failed to initialize client");
        return false;
    }

    while (true)
    {
        if (loopTimer.CheckForUpdate())
        {
            activeClient.SubmitEvent(std::make_shared<AppUpdateEvent>(loopTimer.GetConstantFrameTimeFloat()));
        }
    }

    activeClient.WaitOnClientTerminate();

    return true;
}

int main(int argc, char* argv[])
{
    // Handle command line arguments
    std::optional<AppType> appTypeRef = HandleCMDArguments(argc, argv);
    if (!appTypeRef)
    {
        TSLogger::Log("Retrieve app type from command line arguments");
        return false;
    }

    // Set up config information
    NetworkConfig config;
    config.m_ServerAddress.SetAddress(127, 0, 0, 1);
    config.m_ServerAddress.SetNewPort(3'000);
    config.m_AppProtocolID = 201;


    // Open either the server or the client
    if (*appTypeRef == AppType::Server)
    {
        return !OpenServer(config);
    }
    else if (*appTypeRef == AppType::Client)
    {
        return !OpenClient(config);
    }
    

}