#pragma once
#include "Common.h"
#include "Address.h"
#include "../Util/Logger.h"

class Socket
{
public:
	Socket() = default;
	~Socket() = default;


	bool Open(unsigned short port);
	void Close();
	bool IsOpen() const;
	bool Send(const Address& destination, const void* data, int size);
	int Receive(Address& sender, void* data, int size);
private:
	int handle;
};

//===========================
// Socket Layer Lifecycle
//===========================

inline bool InitializeSockets()
{
#if PLATFORM == PLATFORM_WINDOWS
	WSADATA WsaData;
	return WSAStartup(MAKEWORD(2, 2), &WsaData) == NO_ERROR;
#else
	return true;
#endif
}

inline void ShutdownSockets()
{
#if PLATFORM == PLATFORM_WINDOWS
	WSACleanup();
#endif
}