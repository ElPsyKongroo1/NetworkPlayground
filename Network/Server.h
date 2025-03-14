#pragma once

#include "../Util/Thread.h"
#include "../Posix/Socket.h"
#include "../Util/LoopTimer.h"
#include "../Util/PassiveLoopTimer.h"
#include "../Posix/Connection.h"
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
	bool TerminateServer();
public:
	//==============================
	// Run Threads
	//==============================
	// Run regular server thread
	void RunMainThread();
	// Run socket/packet handling thread
	void RunNetworkThread();

	//==============================
	// Send Packets
	//==============================
	bool SendToConnection(Address connectionAddress, PacketType type, const void* data, int size);
	bool SendToAllConnections(PacketType type, const void* data, int size);
private:
	//==============================
	// Internal Data
	//==============================
	Socket m_ServerSocket;
	NetworkConfig m_Config;
	KGThread m_NetworkThread;
	LoopTimer m_NetworkThreadTimer;
	PassiveLoopTimer m_SyncTimer;

	ConnectionMap m_AllConnections;
};