#pragma once

#include "../Util/Thread.h"
#include "../Posix/Socket.h"
#include "NetworkConfig.h"
#include "../Posix/Connection.h"
#include "../Util/LoopTimer.h"
#include "../Util/PassiveLoopTimer.h"
#include "../Util/EventQueue.h"

enum ConnectionStatus : uint8_t
{
	Disconnected,
	Connecting,
	Connected
};

class ConnectionToServer
{
public:
	//==============================
	// Lifecycle Functions
	//==============================
	void Init(const NetworkConfig& config);
	void Terminate();
public:
	//==============================
	// Public Fields
	//==============================
	Connection m_Connection{};
	ClientIndex m_ClientIndex{};
	ConnectionStatus m_Status{ Disconnected };
};

class Client
{
public:
	//==============================
	// Constructors/Destructors
	//==============================
	Client() = default;
	~Client() = default;

	//==============================
	// Lifecycle Functions
	//==============================
	bool InitClient(const NetworkConfig& initConfig);
	bool TerminateClient();

	// Allows other threads to wait on the client to close
	void WaitOnClientTerminate();

	//==============================
	// On Event
	//==============================
	void OnEvent(Event* event);

	//==============================
	// Manage Events
	//==============================
	void SubmitEvent(Ref<Event> event);
private:
	// Manage the server connection
	void RequestConnection();
public:
	//==============================
	// Run Threads
	//==============================
	// Run socket/packet handling thread
	void RunNetworkThread();
	void RunNetworkEventThread();

private:
	// Helper functions
	bool HandleConsoleInput(KeyPressedEvent event);
public:
	//==============================
	// Send Packets
	//==============================
	bool SendToServer(PacketType type, const void* payload, int payloadSize);
private:
	//==============================
	// Internal Data
	//==============================
	Socket m_ClientSocket;
	KGThread m_NetworkThread;
	KGThread m_NetworkEventThread;
	NetworkConfig m_Config;
	LoopTimer m_NetworkThreadTimer;
	PassiveLoopTimer m_RequestConnectionTimer;
	PassiveLoopTimer m_KeepAliveTimer;
	EventQueue m_NetworkEventQueue;

	// Server connection
	ConnectionToServer m_ServerConnection;
	
};