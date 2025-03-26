#include "Server.h"
#include "../Util/Helper.h"
#include "../Util/StringOperations.h"

#include <conio.h>
#include <queue>
#include <atomic>


static HANDLE hNetworkEvent;
static HANDLE hInputEvent;
static HANDLE allEvents[2];
static std::string text;

bool Server::InitServer(const NetworkConfig& initConfig)
{
    // Set config
    m_Config = initConfig;

    // Initialize the OS specific socket context
    if (!SocketContext::InitializeSockets())
    {
        TSLogger::Log("Failed to initialize platform socket context\n");
        return false;
    }

    // Open the server socket
    if (!m_ServerSocket.Open(initConfig.m_ServerAddress.GetPort()))
    {
        TSLogger::Log("Failed to create socket!\n");
        SocketContext::ShutdownSockets();
        return false;
    }

    m_NetworkEventQueue.Init(KG_BIND_CLASS_FN(OnEvent));

    // TODO: Move this please for the love of god
    // Create network event
    hNetworkEvent = WSACreateEvent();
    if (WSAEventSelect(m_ServerSocket.GetHandle(), hNetworkEvent, FD_READ) != 0)
    {
        TSLogger::Log("Failed to create the network event handle");
        return false;
    }

    // Get console input handle
    hInputEvent = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hInputEvent, 0);

    // Wait for both events
    allEvents[0] = hNetworkEvent;
    allEvents[1] = hInputEvent;

    m_AllConnections = ConnectionList(64);

    m_ManageConnections = false;

    // Start network thread
    m_ManageConnectionTimer.InitializeTimer();
    m_KeepAliveTimer.InitializeTimer(m_Config.m_SyncPingFrequency);
    m_NetworkThread.StartThread(KG_BIND_CLASS_FN(RunNetworkThread));
    m_NetworkEventThread.StartThread(KG_BIND_CLASS_FN(RunNetworkEventThread));

    return true;
}

bool Server::TerminateServer(bool withinNetworkThread)
{
    // Join the network thread
    m_NetworkThread.StopThread(withinNetworkThread);
    m_NetworkEventThread.StopThread(withinNetworkThread);

    m_ManageConnections = false;

    // Clean up socket resources
    SocketContext::ShutdownSockets();

    return true;
}

void Server::WaitOnServerTerminate()
{
    m_NetworkThread.WaitOnThread();
    m_NetworkEventThread.WaitOnThread();
}


void Server::RunNetworkThread()
{
    // Run functions that manage the upkeep of active client connections
    if (m_ManageConnections && !ManageConnections())
    {
        return;
    }

    m_NetworkEventQueue.ProcessQueue();

    Address sender;
    unsigned char buffer[k_MaxPacketSize];
    int bytes_read{ 0 };

    do 
    {
        bytes_read = m_ServerSocket.Receive(sender, buffer, sizeof(buffer));

        if (bytes_read >= (k_PacketHeaderSize))
        {
            // Check for a valid app ID
            if (*(AppID*)&buffer != m_Config.m_AppProtocolID)
            {
                TSLogger::Log("Failed to validate the app ID from packet\n");
                continue;
            }

            // Get the packet type
            PacketType type = (PacketType)buffer[sizeof(AppID)];

            ClientIndex index = (ClientIndex)buffer[sizeof(AppID) + sizeof(PacketType)];

            // Handle messages for already connected clients
            if (m_AllConnections.IsConnectionActive(index))
            {
                // Get the indicated connection
                Connection* connection = m_AllConnections.GetConnection(index);
                KG_ASSERT(connection);

                // Process packet reliability
                connection->m_ReliabilityContext.ProcessReliabilitySegmentFromPacket(&buffer[sizeof(AppID) + sizeof(PacketType) + sizeof(ClientIndex)]);

                switch (type)
                {
                case PacketType::KeepAlive:
                {
                    Connection* clientConnection = m_AllConnections.GetConnection(index);
                    if (!clientConnection)
                    {
                        TSLogger::Log("Failed to get connection object when receiving a keep alive packet\n");
                        continue;
                    }

                    // Reset connection
                    return;
                }
                case PacketType::ConnectionRequest:
                    continue;
                case PacketType::Message:
                {
                    bool valid = isValidCString((char*)buffer + k_PacketHeaderSize);
                    if (!valid)
                    {
                        TSLogger::Log("Buffer could not be converted into a c-string\n");
                        continue;
                    }

                    TSLogger::Log("[%i.%i.%i.%i:%i]: ", sender.GetA(), sender.GetB(),
                        sender.GetC(), sender.GetD(), sender.GetPort());
                    TSLogger::Log("%s", buffer + k_PacketHeaderSize);
                    TSLogger::Log("\n");
                    continue;
                }
                default:
                    TSLogger::Log("Invalid packet ID obtained\n");
                    continue;
                }
            }
            else
            {
                // Handle new connections
                if (type == PacketType::ConnectionRequest)
                {
                    ClientIndex connectionIndex = m_AllConnections.AddConnection(sender);

                    // TODO: Handle rejection case better
                    if (connectionIndex == k_InvalidClientIndex)
                    {
                        return;
                    }

                    if (!m_ManageConnections && m_AllConnections.GetNumberOfClients() > 0)
                    {
                        m_ManageConnections = true;
                        m_ManageConnectionTimer.InitializeTimer();
                        m_KeepAliveTimer.InitializeTimer();
                    }

                    // Get the connection reference
                    Connection* newConnection = m_AllConnections.GetConnection(connectionIndex);

                    if (newConnection)
                    {
                        TSLogger::Log("New connection created\n");
                        SendToConnection(connectionIndex, PacketType::ConnectionSuccess, nullptr, 0);
                    }
                }
            }
        }
    } while (bytes_read > 0);

    // Allow the thread to sleep if not managing connections
    if (!m_ManageConnections)
    {
        m_NetworkThread.SuspendThread(true);
    }
    
}

void Server::RunNetworkEventThread()
{
    DWORD waitResult = WaitForMultipleObjects(2, allEvents, FALSE, INFINITE);

    if (waitResult == WAIT_OBJECT_0)  // Network event
    {
        WSANETWORKEVENTS netEvents;
        WSAEnumNetworkEvents(m_ServerSocket.GetHandle(), hNetworkEvent, &netEvents);

        if (netEvents.lNetworkEvents & FD_READ)
        {
            m_NetworkThread.ResumeThread();
        }
    }
    else if (waitResult == WAIT_OBJECT_0 + 1)  // Console input event
    {
        INPUT_RECORD inputRecord;
        DWORD eventsRead;

        while (true)
        {
            DWORD numEvents;
            if (!GetNumberOfConsoleInputEvents(hInputEvent, &numEvents) || numEvents == 0)
                break;  // No more events, exit the loop

            if (ReadConsoleInput(hInputEvent, &inputRecord, 1, &eventsRead))
            {
                if (inputRecord.EventType == KEY_EVENT && inputRecord.Event.KeyEvent.bKeyDown)
                {
                    char key = inputRecord.Event.KeyEvent.uChar.AsciiChar;

                    m_NetworkEventQueue.SubmitEvent(std::make_shared<KeyPressedEvent>(key));
                    m_NetworkThread.ResumeThread();
                }
            }
        }
    }
}

bool Server::ManageConnections()
{
    if (!m_ManageConnectionTimer.CheckForUpdate())
    {
        return false;
    }

    if (m_KeepAliveTimer.CheckForUpdate(m_ManageConnectionTimer.GetConstantFrameTime()))
    {
        static uint32_t s_CongestionCounter{ 0 };

        if (s_CongestionCounter % 3 == 0)
        {
            // Send keep-alive to all connections
            SendToAllConnections(PacketType::KeepAlive, nullptr, 0);
        }
        else
        {
            // Only send keep alive to non-congested connections
            ClientIndex index{ 0 };
            for (Connection& connection : m_AllConnections.GetAllConnections())
            {
                if (!m_AllConnections.IsConnectionActive(index))
                {
                    continue;
                }

                if (!connection.m_ReliabilityContext.m_CongestionContext.IsCongested())
                {
                    SendToConnection(index, PacketType::KeepAlive, nullptr, 0);
                }
                index++;
            }
        }

        s_CongestionCounter++;
    }

    // Add delta-time to last-packet-received time for all connections
    std::vector<ClientIndex> clientsToRemove;
    ClientIndex index{ 0 };
    for (Connection& connection : m_AllConnections.GetAllConnections())
    {
        if (!m_AllConnections.IsConnectionActive(index))
        {
            continue;
        }

        connection.m_ReliabilityContext.OnUpdate(m_ManageConnectionTimer.GetConstantFrameTimeFloat());

        if (connection.m_ReliabilityContext.m_LastPacketReceived > m_Config.m_ConnectionTimeout)
        {
            clientsToRemove.push_back(index);
        }
        index++;
    }

    // Remove timed-out connections
    for (ClientIndex index : clientsToRemove)
    {
        TSLogger::Log("Removing client");
        m_AllConnections.RemoveConnection(index);
    }

    if (m_AllConnections.GetNumberOfClients() <= 0)
    {
        m_ManageConnections = false;
    }

    return true;
}

void Server::HandleConsoleInput(KeyPressedEvent event)
{
    char key = event.GetKeyCode();
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
        TerminateServer(true);
        return;
    }
    if (key == 13)
    {
        SendToAllConnections(PacketType::Message, text.data(), (int)strlen(text.data()) + 1);
        text.clear();
    }
    
    return;
}

void Server::OnEvent(Event* event)
{
    if (event->GetEventType() == EventType::KeyPressed)
    {
        HandleConsoleInput(*(KeyPressedEvent*)event);
    }
}


void Server::SubmitEvent(Ref<Event> event)
{
    m_NetworkEventQueue.SubmitEvent(event);

    m_NetworkEventThread.ResumeThread();
}

bool Server::SendToConnection(ClientIndex clientIndex, PacketType type, const void* payload, int payloadSize)
{
    // Get the connection
    Connection* connection = m_AllConnections.GetConnection(clientIndex);

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

    // Set the packet type
    PacketType& packetTypeLocation = *(PacketType*)&buffer[sizeof(AppID)];
    packetTypeLocation = type;

    // Send the client connection Index
    ClientIndex& clientIndexLocation = *(ClientIndex*)&buffer[sizeof(AppID) + sizeof(PacketType)];
    clientIndexLocation = clientIndex;

    if (!IsConnectionManagementPacket(type))
    {
        // Insert the sequence number + ack + ack_bitfield
        connection->m_ReliabilityContext.InsertReliabilitySegmentIntoPacket(&buffer[sizeof(AppID) + sizeof(PacketType)]);
    }

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
    ClientIndex currentIndex{ 0 };
    for (Connection& connection : m_AllConnections.GetAllConnections())
    {
        if (!m_AllConnections.IsConnectionActive(currentIndex))
        {
            currentIndex++;
            continue;
        }

        // Prepare the final data buffer
        uint8_t buffer[k_MaxPacketSize];

        // Set the app ID
        AppID& appIDLocation = *(AppID*)&buffer[0];
        appIDLocation = m_Config.m_AppProtocolID;

        // Set the packet type
        PacketType& packetTypeLocation = *(PacketType*)&buffer[sizeof(AppID)];
        packetTypeLocation = type;

        if (!IsConnectionManagementPacket(type))
        {
            // Insert the sequence number + ack + ack_bitfield
            connection.m_ReliabilityContext.InsertReliabilitySegmentIntoPacket(&buffer[sizeof(AppID) + sizeof(PacketType) + sizeof(ClientIndex)]);
        }

        if (payloadSize > 0)
        {
            // Set the data
            memcpy(&buffer[k_PacketHeaderSize], payload, payloadSize);
        }

        m_ServerSocket.Send(connection.m_Address, buffer, k_PacketHeaderSize + payloadSize);

        currentIndex++;
    }

    return true;
}
