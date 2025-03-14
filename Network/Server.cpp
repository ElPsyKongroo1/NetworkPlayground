#include "Server.h"
#include "../Util/Helper.h"
#include "../Util/StringOperations.h"

#include <conio.h>

bool Server::InitServer(const NetworkConfig& initConfig)
{
    // Set config
    m_Config = initConfig;

    // Initialize the OS specific socket context
    InitializeSockets();

    // Open the server socket
    if (!m_ServerSocket.Open(initConfig.m_ServerAddress.GetPort()))
    {
        TSLogger::Log("Failed to create socket!\n");
        ShutdownSockets();
        return false;
    }

    // Start network thread
    m_NetworkThreadTimer.InitializeTimer();
    m_SyncTimer.InitializeTimer(m_Config.m_SyncPingFrequency);
    m_NetworkThread.StartThread(KG_BIND_CLASS_FN(RunNetworkThread));

    return true;
}

bool Server::TerminateServer()
{
    // Join the network thread
    m_NetworkThread.StopThread();

    // Clean up socket resources
    ShutdownSockets();

    return true;
}

void Server::RunMainThread()
{
    std::string text;
    while (true)
    {
        if (_kbhit())
        {
            char key = _getch();

            if (key >= 32 && key < 127)
            {
                text += key;
                TSLogger::Log("%c", key);
            }
            if (key == 127 && text.size() > 0)
            {
                TSLogger::Log("\b \b");
                text.pop_back();
            }
            if (key == 27) // Escape key
            {
                break;
            }
            if (key == 13)
            {
                m_NetworkThread.BlockThread();
                SendToAllConnections(PacketType::Message, text.data(), (int)strlen(text.data()) + 1);
                m_NetworkThread.ResumeThread();
                text.clear();
            }
        }
    }
}

void Server::RunNetworkThread()
{
    if (!m_NetworkThreadTimer.CheckForUpdate())
    {
        return;
    }

    if (m_SyncTimer.CheckForUpdate(m_NetworkThreadTimer.GetConstantFrameTime()))
    {
        // Send synchronization pings
        SendToAllConnections(PacketType::KeepAlive, nullptr, 0);
    }

    // Add delta-time to last-packet-received time for all connections
    std::vector<Address> addressesToRemove;
    for (auto& [address, connection] : m_AllConnections.GetMap())
    {
        connection.m_LastPacketReceived += m_NetworkThreadTimer.GetConstantFrameTimeFloat();
        if (connection.m_LastPacketReceived > m_Config.m_ConnectionTimeout)
        {
            addressesToRemove.push_back(address);
        }
    }

    // Remove timed-out connections
    for (Address address : addressesToRemove)
    {
        TSLogger::Log("Removing client");
        m_AllConnections.RemoveConnection(address);
    }

    Address sender;
    unsigned char buffer[k_MaxPacketSize];
    int bytes_read = m_ServerSocket.Receive(sender, buffer, sizeof(buffer));

    if (bytes_read >= (sizeof(AppID) + sizeof(PacketType)))
    {
        // Check for a valid app ID
        if (*(AppID*)&buffer != m_Config.m_AppProtocolID)
        {
            TSLogger::Log("Failed to validate the app ID from packet\n");
            return;
        }


        PacketType type = (PacketType)buffer[sizeof(AppID)];

        // Handle messages for already connected clients
        if (m_AllConnections.IsConnectionActive(sender))
        {
            switch (type)
            {
            case PacketType::KeepAlive:
            {
                Connection* clientConnection = m_AllConnections.GetConnection(sender);
                if (!clientConnection)
                {
                    TSLogger::Log("Failed to get connection object when receiving a keep alive packet\n");
                    return;
                }

                // Reset connection
                clientConnection->m_LastPacketReceived = 0;
                return;
            }
            case PacketType::ConnectionRequest:
                return;
            case PacketType::Message:
            {
                bool valid = isValidCString((char*)buffer + sizeof(AppID) + sizeof(PacketType));
                if (!valid)
                {
                    TSLogger::Log("Buffer could not be converted into a c-string\n");
                    return;
                }

                TSLogger::Log("\n");
                TSLogger::Log("[%i.%i.%i.%i:%i]: ", sender.GetA(), sender.GetB(),
                    sender.GetC(), sender.GetD(), sender.GetPort());
                TSLogger::Log("%s", buffer + sizeof(AppID) + sizeof(PacketType));
                TSLogger::Log("\n");
                return;
            }
            default:
                TSLogger::Log("Invalid packet ID obtained\n");
                return;
            }
        }
        else
        {
            // Handle new connections
            if (type == PacketType::ConnectionRequest)
            {
                Connection* newConnection = m_AllConnections.AddConnection(sender);
                if (newConnection)
                {
                    TSLogger::Log("New connection created\n");
                    SendToConnection(newConnection->m_Address, PacketType::ConnectionSuccess, nullptr, 0);
                }
                else
                {
                    TSLogger::Log("Failed to create new connection\n");
                    SendToConnection(sender, PacketType::ConnectionDenied, nullptr, 0);
                }
            }
        }
    }

    // process packet
}

bool Server::SendToConnection(Address connectionAddress, PacketType type, const void* payload, int payloadSize)
{
    if (payloadSize >= k_MaxPayloadSize)
    {
        TSLogger::Log("Failed to send packet. Payload exceeds maximum size limit\n");
        return false;
    }

    // Prepare the final data buffer
    uint8_t buffer[k_MaxPacketSize];

    // Set the app ID
    AppID& appIDLocation = *(AppID*)&buffer[0];
    appIDLocation = m_Config.m_AppProtocolID;

    // Set the packet type
    PacketType& packetTypeLocation = *(PacketType*)&buffer[sizeof(AppID)];
    packetTypeLocation = type;

    if (payloadSize > 0)
    {
        // Set the data
        memcpy(&buffer[sizeof(AppID) + sizeof(PacketType)], payload, payloadSize);
    }

    m_ServerSocket.Send(connectionAddress, buffer, payloadSize + sizeof(AppID) + sizeof(PacketType));

    return true;
}

bool Server::SendToAllConnections(PacketType type, const void* payload, int payloadSize)
{
    if (payloadSize >= k_MaxPayloadSize)
    {
        TSLogger::Log("Failed to send packet. Payload exceeds maximum size limit\n");
        return false;
    }

    // Prepare the final data buffer
    uint8_t buffer[k_MaxPacketSize];

    // Set the app ID
    AppID& appIDLocation = *(AppID*)&buffer[0];
    appIDLocation = m_Config.m_AppProtocolID;

    // Set the packet type
    PacketType& packetTypeLocation = *(PacketType*)&buffer[sizeof(AppID)];
    packetTypeLocation = type;

    if (payloadSize > 0)
    {
        // Set the data
        memcpy(&buffer[sizeof(AppID) + sizeof(PacketType)], payload, payloadSize);
    }

    for (auto& [address, connection] : m_AllConnections.GetMap())
    {
        m_ServerSocket.Send(address, buffer, payloadSize + sizeof(AppID) + sizeof(PacketType));
    }

    return true;
}
