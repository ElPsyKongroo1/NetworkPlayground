#include "Client.h"
#include "../Util/Helper.h"
#include "../Util/StringOperations.h"

#include <conio.h>

bool Client::InitClient(const NetworkConfig& initConfig)
{
    // Set config
    m_Config = initConfig;

    // Initialize the OS specific socket context
    InitializeSockets();

    // Open the Client socket
    if (!m_ClientSocket.Open(initConfig.m_ServerAddress.GetPort() + 1))
    {
        TSLogger::Log("Failed to create socket!\n");
        ShutdownSockets();
        return false;
    }

    // Initialize server connection
    InitializeServerConnection();

    // Request joining the server
    if (!RequestConnection())
    {
        TSLogger::Log("Failed to connect to the server\n");
        return false;
    }
    
    // Start network thread
    m_NetworkThreadTimer.InitializeTimer();
    m_SyncTimer.InitializeTimer(m_Config.m_SyncPingFrequency);
    m_NetworkThread.StartThread(KG_BIND_CLASS_FN(RunNetworkThread));

    return true;
}

bool Client::TerminateClient()
{
    // Join the network thread
    m_NetworkThread.StopThread();

    // Clean up socket resources
    ShutdownSockets();

    return true;
}

void Client::InitializeServerConnection()
{
    m_ServerConnection.m_Address = m_Config.m_ServerAddress;
    m_ServerConnection.m_LastPacketReceived = 0.0f;
}

void Client::TerminateServerConnection()
{
    m_ServerConnection.m_Address = Address();
    m_ServerConnection.m_LastPacketReceived = 0.0f;
}

void Client::RunMainThread()
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
                SendToServer(PacketType::Message, text.data(), (int)strlen(text.data()) + 1);
                m_NetworkThread.ResumeThread();
                text.clear();
            }
        }
    }
}

void Client::RunNetworkThread()
{
    // Check for a network update
    if (!m_NetworkThreadTimer.CheckForUpdate())
    {
        return;
    }

    // Handle sync pings
    if (m_SyncTimer.CheckForUpdate(m_NetworkThreadTimer.GetConstantFrameTime()))
    {
        // Send synchronization pings
        SendToServer(PacketType::KeepAlive, nullptr, 0);

    }

    // Increment time since last sync ping for server connection
    m_ServerConnection.m_LastPacketReceived += m_NetworkThreadTimer.GetConstantFrameTimeFloat();
    if (m_ServerConnection.m_LastPacketReceived > m_Config.m_ConnectionTimeout)
    {
        TerminateServerConnection();
        m_NetworkThread.StopThread(true);
        TSLogger::Log("Connection closed\n");
        return;
    }
    

    Address sender;
    unsigned char buffer[k_MaxPacketSize];
    int bytes_read = m_ClientSocket.Receive(sender, buffer, sizeof(buffer));

    if (bytes_read >= (sizeof(AppID) + sizeof(PacketType)))
    {
        // Check for a valid app ID
        if (*(AppID*)&buffer != m_Config.m_AppProtocolID)
        {
            TSLogger::Log("Failed to validate the app ID from packet\n");
            return;
        }
        
        PacketType type = (PacketType)buffer[sizeof(AppID)];

        switch (type)
        {
        case PacketType::KeepAlive:
            //TSLogger::Log("Received keep alive packet\n");
            m_ServerConnection.m_LastPacketReceived = 0.0f;
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
            break;
        }
        default:
            TSLogger::Log("Invalid packet ID obtained");
            return;
        }
    }

    // process packet
}

bool Client::RequestConnection()
{
    LoopTimer loopTimer;
    PassiveLoopTimer requestConnectTimer;

    // Initialize local timers
    loopTimer.InitializeTimer();
    requestConnectTimer.InitializeTimer(m_Config.m_RequestConnectionFrequency);

    // Send initial connection request
    SendToServer(PacketType::ConnectionRequest, nullptr, 0);

    while (true)
    {
        // Check for a network update
        if (!loopTimer.CheckForUpdate())
        {
            continue;
        }

        // Handle connection requests
        if (requestConnectTimer.CheckForUpdate(loopTimer.GetConstantFrameTime()))
        {
            // Send connection request
            SendToServer(PacketType::ConnectionRequest, nullptr, 0);
        }

        // Increment time since start of connection attempt
        m_ServerConnection.m_LastPacketReceived += loopTimer.GetConstantFrameTimeFloat();
        if (m_ServerConnection.m_LastPacketReceived > m_Config.m_ConnectionTimeout)
        {
            return false;
        }

        Address sender;
        unsigned char buffer[k_MaxPacketSize];
        int bytes_read = m_ClientSocket.Receive(sender, buffer, sizeof(buffer));

        if (bytes_read >= (sizeof(AppID) + sizeof(PacketType)))
        {
            // Check for a valid app ID
            if (*(AppID*)&buffer != m_Config.m_AppProtocolID)
            {
                TSLogger::Log("Failed to validate the app ID from packet\n");
                continue;
            }

            PacketType type = (PacketType)buffer[sizeof(AppID)];

            if (type == PacketType::ConnectionSuccess)
            {
                TSLogger::Log("Connection successful!\n");
                return true;
            }
            else if (type == PacketType::ConnectionDenied)
            {
                TSLogger::Log("Connection denied!\n");
                return false;
            }
        }
    }
}

bool Client::SendToServer(PacketType type, const void* payload, int payloadSize)
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
       // Set the payload data
       memcpy(&buffer[sizeof(AppID) + sizeof(PacketType)], payload, payloadSize);
   }

   // Send the message
   bool sendSuccess{ false };
   sendSuccess = m_ClientSocket.Send(m_ServerConnection.m_Address, buffer, payloadSize + sizeof(AppID) + sizeof(PacketType));

   return sendSuccess;
}
