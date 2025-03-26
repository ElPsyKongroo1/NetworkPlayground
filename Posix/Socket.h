#pragma once
#include "PosixImpl.h"
#include "Address.h"
#include "../Util/Logger.h"

class Socket
{
public:
	//==============================
	// Constructors/Destructors
	//==============================
	Socket() = default;
	~Socket() = default;

	//==============================
	// Lifecycle Functions
	//==============================
	bool Open(unsigned short m_Port);
	void Close();

	//==============================
	// Send/Receive Messages
	//==============================
	bool Send(const Address& destination, const void* data, int size);
	int Receive(Address& sender, void* data, int size);

	//==============================
	// Query Socket State
	//==============================
	bool IsOpen() const;

	//==============================
	// Getters/Setters
	//==============================
	int GetHandle() const;
private:
	//==============================
	// Internal Fields
	//==============================
	int m_Handle;
};

//===========================
// Socket Context
//===========================

class SocketContext 
{
public:
	//==============================
	// Lifecycle Functions
	//==============================
	static bool InitializeSockets();
	static void ShutdownSockets();
};