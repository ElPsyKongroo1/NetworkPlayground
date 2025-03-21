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
    m_KeepTimer.InitializeTimer(m_Config.m_SyncPingFrequency);
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

    
    if (m_KeepTimer.CheckForUpdate(m_NetworkThreadTimer.GetConstantFrameTime()))
    {
        static uint32_t s_CongestionCounter{0};

        if (s_CongestionCounter % 3 == 0)
        {
            // Send keep-alive to all connections
            SendToAllConnections(PacketType::KeepAlive, nullptr, 0);
        }
        else
        {
            // Only send keep alive to non-congested connections
            for (auto& [address, connection] : m_AllConnections.GetMap())
            {
                if (!connection.m_ReliabilityContext.IsCongested())
                {
                    SendToConnection(&connection, PacketType::KeepAlive, nullptr, 0);
                }
            }
        }

        s_CongestionCounter++;
    }

    // Add delta-time to last-packet-received time for all connections
    std::vector<Address> addressesToRemove;
    for (auto& [address, connection] : m_AllConnections.GetMap())
    {
        connection.m_ReliabilityContext.UpdateLastPacketReceived(m_NetworkThreadTimer.GetConstantFrameTimeFloat());
        if (connection.m_ReliabilityContext.GetLastPacketReceived() > m_Config.m_ConnectionTimeout)
        {
            addressesToRemove.push_back(address);
        }

        connection.m_ReliabilityContext.UpdateTimeInCongestionState(m_NetworkThreadTimer.GetConstantFrameTimeFloat());
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

    if (bytes_read >= (k_PacketHeaderSize))
    {
        // Check for a valid app ID
        if (*(AppID*)&buffer != m_Config.m_AppProtocolID)
        {
            TSLogger::Log("Failed to validate the app ID from packet\n");
            return;
        }

        // Get the packet type
        PacketType type = (PacketType)buffer[sizeof(AppID) + k_ReliabilitySegmentSize];

        // Handle messages for already connected clients
        if (m_AllConnections.IsConnectionActive(sender))
        {
            // Get the indicated connection
            Connection* connection = m_AllConnections.GetConnection(sender);
            KG_ASSERT(connection);

            // Process packet reliability
            connection->m_ReliabilityContext.ProcessReliabilitySegmentFromPacket(&buffer[sizeof(AppID)]);

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
                return;
            }
            case PacketType::ConnectionRequest:
                return;
            case PacketType::Message:
            {
                bool valid = isValidCString((char*)buffer + k_PacketHeaderSize);
                if (!valid)
                {
                    TSLogger::Log("Buffer could not be converted into a c-string\n");
                    return;
                }

                TSLogger::Log("[%i.%i.%i.%i:%i]: ", sender.GetA(), sender.GetB(),
                    sender.GetC(), sender.GetD(), sender.GetPort());
                TSLogger::Log("%s", buffer + k_PacketHeaderSize);
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

                // Process packet reliability
                newConnection->m_ReliabilityContext.ProcessReliabilitySegmentFromPacket(&buffer[sizeof(AppID)]);

                if (newConnection)
                {
                    TSLogger::Log("New connection created\n");
                    SendToConnection(newConnection, PacketType::ConnectionSuccess, nullptr, 0);
                }
            }
        }
    }

    // process packet
}

bool Server::SendToConnection(Connection* connection, PacketType type, const void* payload, int payloadSize)
{
    if (!connection)
    {
        TSLogger::Log("Failed to send message to connection. Invalid connection context provided\n");
        return false;
    }

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

    // Insert the sequence number + ack + ack_bitfield
    connection->m_ReliabilityContext.InsertReliabilitySegmentIntoPacket(&buffer[sizeof(AppID)]);

    // Set the packet type
    PacketType& packetTypeLocation = *(PacketType*)&buffer[sizeof(AppID) + k_ReliabilitySegmentSize];
    packetTypeLocation = type;

    // Optionally insert the payload
    if (payloadSize > 0)
    {
        // Set the data
        memcpy(&buffer[k_PacketHeaderSize], payload, payloadSize);
    }

    m_ServerSocket.Send(connection->m_Address, buffer, 
        payloadSize + k_PacketHeaderSize);

    return true;
}

bool Server::SendToAllConnections(PacketType type, const void* payload, int payloadSize)
{
    // Check the payload size is valid
    if (payloadSize >= k_MaxPayloadSize)
    {
        TSLogger::Log("Failed to send packet. Payload exceeds maximum size limit\n");
        return false;
    }

    // Loop through all of the connections
    for (auto& [address, connection] : m_AllConnections.GetMap())
    {
        // Prepare the final data buffer
        uint8_t buffer[k_MaxPacketSize];

        // Set the app ID
        AppID& appIDLocation = *(AppID*)&buffer[0];
        appIDLocation = m_Config.m_AppProtocolID;

        // Insert the sequence number + ack + ack_bitfield
        connection.m_ReliabilityContext.InsertReliabilitySegmentIntoPacket(&buffer[sizeof(AppID)]);

        // Set the packet type
        PacketType& packetTypeLocation = *(PacketType*)&buffer[sizeof(AppID) + k_ReliabilitySegmentSize];
        packetTypeLocation = type;

        if (payloadSize > 0)
        {
            // Set the data
            memcpy(&buffer[k_PacketHeaderSize], payload, payloadSize);
        }

        m_ServerSocket.Send(address, buffer, k_PacketHeaderSize + payloadSize);
    }

    return true;
}
