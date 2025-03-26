#pragma once

#include "../Util/Thread.h"
#include "../Posix/Socket.h"
#include "../Util/LoopTimer.h"
#include "../Util/PassiveLoopTimer.h"
#include "../Posix/Connection.h"
#include "../Util/EventQueue.h"
#include "NetworkConfig.h"

class Server 
{
public:
	//==============================
	// Constructors/Destructors
	//==============================
	Server() = default;
	~Server() = default;

	//==============================
	// Lifecycle Functions
	//==============================
	bool InitServer(const NetworkConfig& initConfig);
	bool TerminateServer(bool withinNetworkThread = false);

	// Allows other threads to wait on the server to close
	void WaitOnServerTerminate();
public:
	//==============================
	// Run Threads
	//==============================
	// Run socket/packet handling thread
	void RunNetworkThread();
	void RunNetworkEventThread();

private:
	// Helper functions
	bool ManageConnections();
	void HandleConsoleInput(KeyPressedEvent event);

public:
	//==============================
	// On Event
	//==============================
	void OnEvent(Event* event);

	//==============================
	// Manage Events
	//==============================
	void SubmitEvent(Ref<Event> event);

	//==============================
	// Send Packets
	//==============================
	bool SendToConnection(ClientIndex clientIndex, PacketType type, const void* data, int size);
	bool SendToAllConnections(PacketType type, const void* data, int size);
private:
	//==============================
	// Internal Data
	//==============================
	bool m_ManageConnections{ false };
	Socket m_ServerSocket;
	NetworkConfig m_Config;
	KGThread m_NetworkThread;
	KGThread m_NetworkEventThread;
	LoopTimer m_ManageConnectionTimer;
	PassiveLoopTimer m_KeepAliveTimer;
	ConnectionList m_AllConnections;
	EventQueue m_NetworkEventQueue;
};