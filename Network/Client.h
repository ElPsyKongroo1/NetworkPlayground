#pragma once

#include "../Util/Thread.h"
#include "../Posix/Socket.h"
#include "NetworkConfig.h"
#include "../Posix/Connection.h"
#include "../Util/LoopTimer.h"
#include "../Util/PassiveLoopTimer.h"

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

private:
	void InitializeServerConnection();
	void TerminateServerConnection();
	bool RequestConnection();
public:
	//==============================
	// Run Threads
	//==============================
	// Run regular Client thread
	void RunMainThread();
	// Run socket/packet handling thread
	void RunNetworkThread();

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
	NetworkConfig m_Config;
	LoopTimer m_NetworkThreadTimer;
	PassiveLoopTimer m_KeepTimer;

	// Server connection
	Connection m_ServerConnection;
	
};