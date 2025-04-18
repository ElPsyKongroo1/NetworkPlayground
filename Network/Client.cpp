#include "Client.h"
#include "../Util/Helper.h"
#include "../Util/StringOperations.h"

#include <conio.h>
#include <queue>

static HANDLE hNetworkEvent;
static HANDLE hInputEvent;
static HANDLE allEvents[2];
static std::string text;

bool Client::InitClient(const NetworkConfig& initConfig)
{
    // Set config
    m_Config = initConfig;

    // Initialize the OS specific socket context
    if (!SocketContext::InitializeSockets())
    {
        TSLogger::Log("Failed to initialize platform socket context\n");
        return false;
    }

    // Open the Client socket
    if (!m_ClientSocket.Open(initConfig.m_ServerAddress.GetPort() + 1))
    {
        TSLogger::Log("Failed to create socket!\n");
        SocketContext::ShutdownSockets();
        return false;
    }

    // Initialize server connection
    m_ServerConnection.Init(initConfig);

    m_NetworkEventQueue.Init(KG_BIND_CLASS_FN(OnEvent));

    // TODO: Move this please for the love of god
    // Create network event
    hNetworkEvent = WSACreateEvent();
    if (WSAEventSelect(m_ClientSocket.GetHandle(), hNetworkEvent, FD_READ) != 0)
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

    // Initialize local timers
    m_NetworkThreadTimer.InitializeTimer();
    m_RequestConnectionTimer.InitializeTimer(m_Config.m_RequestConnectionFrequency);

    // Send initial connection request
    m_ServerConnection.m_Status = ConnectionStatus::Connecting;
    SendToServer(PacketType::ConnectionRequest, nullptr, 0);

    // Start request connection
    m_NetworkThread.StartThread(KG_BIND_CLASS_FN(RequestConnection));

    // Wait for request connection to complete
    m_NetworkThread.WaitOnThread();
    
    // Ensure the connection was successful
    if (m_ServerConnection.m_Status != ConnectionStatus::Connected)
    {
        TSLogger::Log("Failed to connect to the server\n");
        return false;
    }
    
    // Start network thread
    m_NetworkThreadTimer.InitializeTimer();
    m_KeepAliveTimer.InitializeTimer(m_Config.m_SyncPingFrequency);
    m_NetworkThread.StartThread(KG_BIND_CLASS_FN(RunNetworkThread));
    m_NetworkEventThread.StartThread(KG_BIND_CLASS_FN(RunNetworkEventThread));

    return true;
}

bool Client::TerminateClient()
{
    // Join the network thread
    m_NetworkThread.StopThread();

    // Clean up socket resources
    SocketContext::ShutdownSockets();

    return true;
}

void Client::WaitOnClientTerminate()
{
    m_NetworkThread.WaitOnThread();
    m_NetworkEventThread.WaitOnThread();
}

void Client::RunNetworkThread()
{
    // Process the network event queue
    m_NetworkEventQueue.ProcessQueue();

    Address sender;
    unsigned char buffer[k_MaxPacketSize];
    int bytes_read{ 0 };

    do
    {
        bytes_read = m_ClientSocket.Receive(sender, buffer, sizeof(buffer));
        if (bytes_read >= k_PacketHeaderSize)
        {
            // Check for a valid app ID
            if (*(AppID*)&buffer != m_Config.m_AppProtocolID)
            {
                TSLogger::Log("Failed to validate the app ID from packet\n");
                continue;
            }

            PacketType type = (PacketType)buffer[sizeof(AppID)];

            if (IsConnectionManagementPacket(type))
            {
                continue;
            }

            // TODO: Verify this message is for the correct client
            ClientIndex index = (ClientIndex)buffer[sizeof(AppID) + sizeof(PacketType)];

            // Process reliability segment
            m_ServerConnection.m_Connection.m_ReliabilityContext.ProcessReliabilitySegmentFromPacket(&buffer[sizeof(AppID) + sizeof(PacketType) + sizeof(ClientIndex)]);


            switch (type)
            {
            case PacketType::KeepAlive:
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
                break;
            }
            default:
                TSLogger::Log("Invalid packet ID obtained");
                continue;;
            }
        }
    } while (bytes_read > 0);
    
    // Suspend the thread until an event occurs
    m_NetworkThread.SuspendThread(true);

    // process packet
}

void Client::RunNetworkEventThread()
{
    DWORD waitResult = WaitForMultipleObjects(2, allEvents, FALSE, INFINITE);

    if (waitResult == WAIT_OBJECT_0)  // Network event
    {
        WSANETWORKEVENTS netEvents;
        WSAEnumNetworkEvents(m_ClientSocket.GetHandle(), hNetworkEvent, &netEvents);

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

bool Client::HandleConsoleInput(KeyPressedEvent event)
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
        return false;
    }
    if (key == 13)
    {
        SendToServer(PacketType::Message, text.data(), (int)strlen(text.data()) + 1);
        text.clear();
    }
    
    return true;
}

void Client::OnEvent(Event* event)
{
    Connection& connection = m_ServerConnection.m_Connection;
    ReliabilityContext& reliableContext = connection.m_ReliabilityContext;

    if (event->GetEventType() == EventType::AppUpdate)
    {
        // Handle sync pings
        if (m_KeepAliveTimer.CheckForUpdate(m_NetworkThreadTimer.GetConstantFrameTime()))
        {
            if (connection.m_ReliabilityContext.m_CongestionContext.IsCongested())
            {
                static uint32_t s_CongestionCounter{ 0 };
                if (s_CongestionCounter % 3 == 0)
                {
                    // Send synchronization pings
                    SendToServer(PacketType::KeepAlive, nullptr, 0);
                }
                s_CongestionCounter++;
            }
            else
            {
                SendToServer(PacketType::KeepAlive, nullptr, 0);
            }
        }

        // Increment time since last sync ping for server connection
        reliableContext.OnUpdate(m_NetworkThreadTimer.GetConstantFrameTimeFloat());

        if (reliableContext.m_LastPacketReceived > m_Config.m_ConnectionTimeout)
        {
            m_ServerConnection.Terminate();
            m_NetworkThread.StopThread(true);
            TSLogger::Log("Connection closed\n");
            return;
        }

        
    }
    else if (event->GetEventType() == EventType::KeyPressed)
    {
        // Handle any input events from the console
        HandleConsoleInput(*(KeyPressedEvent*)event);
        return;
    }

    
}

void Client::SubmitEvent(Ref<Event> event)
{
    m_NetworkEventQueue.SubmitEvent(event);

    m_NetworkEventThread.ResumeThread();
}

void Client::RequestConnection()
{
    ReliabilityContext& reliabilityContext = m_ServerConnection.m_Connection.m_ReliabilityContext;

    // Check for a network update
    if (!m_NetworkThreadTimer.CheckForUpdate())
    {
        return;
    }

    // Handle sending connection requests
    if (m_RequestConnectionTimer.CheckForUpdate(m_NetworkThreadTimer.GetConstantFrameTime()))
    {
        // Send connection request
        SendToServer(PacketType::ConnectionRequest, nullptr, 0);
    }

    // Increment time since start of connection attempt
    reliabilityContext.m_LastPacketReceived += m_NetworkThreadTimer.GetConstantFrameTimeFloat();
    if (reliabilityContext.m_LastPacketReceived > m_Config.m_ConnectionTimeout)
    {
        m_ServerConnection.m_Status = ConnectionStatus::Disconnected;
        m_NetworkThread.StopThread(true);
        return;
    }

    Address sender;
    unsigned char buffer[k_MaxPacketSize];
    int bytes_read{ 0 };
        
    do
    {
        bytes_read = m_ClientSocket.Receive(sender, buffer, sizeof(buffer));
        if (bytes_read >= k_PacketHeaderSize)
        {
            // Check for a valid app ID
            if (*(AppID*)&buffer != m_Config.m_AppProtocolID)
            {
                TSLogger::Log("Failed to validate the app ID from packet\n");
                continue;
            }

            // Get packet type
            PacketType type = (PacketType)buffer[sizeof(AppID)];

            ClientIndex index = (ClientIndex)buffer[sizeof(AppID) + sizeof(PacketType)];

            if (!IsConnectionManagementPacket(type))
            {
                continue;
            }

            if (type == PacketType::ConnectionSuccess)
            {
                m_ServerConnection.m_Status = ConnectionStatus::Connected;
                m_ServerConnection.m_ClientIndex = index;
                TSLogger::Log("Connection successful!\n");
                m_NetworkThread.StopThread(true);
                return;
            }
            else if (type == PacketType::ConnectionDenied)
            {
                m_ServerConnection.m_Status = ConnectionStatus::Disconnected;
                TSLogger::Log("Connection denied!\n");
                m_NetworkThread.StopThread(true);
                return;
            }
        }
    } while (bytes_read > 0);
    
}

bool Client::SendToServer(PacketType type, const void* payload, int payloadSize)
{
    Connection& connection = m_ServerConnection.m_Connection;

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
   clientIndexLocation = m_ServerConnection.m_ClientIndex;

   if (!IsConnectionManagementPacket(type))
   {
       // Set reliability segment
       connection.m_ReliabilityContext.InsertReliabilitySegmentIntoPacket(&buffer[sizeof(AppID) + sizeof(PacketType) + sizeof(ClientIndex)]);
   }

   if (payloadSize > 0)
   {
       // Set the payload data
       memcpy(&buffer[k_PacketHeaderSize], payload, payloadSize);
   }

   // Send the message
   bool sendSuccess{ false };
   sendSuccess = m_ClientSocket.Send(connection.m_Address, buffer, payloadSize + k_PacketHeaderSize);

   return sendSuccess;
}

void ConnectionToServer::Init(const NetworkConfig& config)
{
    m_Connection.m_Address = config.m_ServerAddress;
    m_Connection.m_ReliabilityContext.m_LastPacketReceived = 0.0f;
    m_Status = ConnectionStatus::Disconnected;
    m_ClientIndex = k_InvalidClientIndex;
}

void ConnectionToServer::Terminate()
{
    m_Connection.m_Address = Address();
    m_Connection.m_ReliabilityContext.m_LastPacketReceived = 0.0f;
    m_Status = ConnectionStatus::Disconnected;
    m_ClientIndex = k_InvalidClientIndex;
}
