#include "Socket.h"
#include "stdio.h"

bool Socket::Open(unsigned short port)
{
	// Create the UDP socket
	handle = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	// Ensure the socket is created
	if (handle == -1)
	{
		TSLogger::Log("Failed to create a socket: %d\n", WSAGetLastError());
		return false;
	}

	// Create the socket's address
	sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons((unsigned short)port);

	// Bind the address to the socket
	if (bind(handle, (const sockaddr*)&address, sizeof(sockaddr_in)) < 0)
	{
		TSLogger::Log("Failed to bind socket \n");
		return false;
	}

	// Set socket to non-blocking mode
#if PLATFORM == PLATFORM_MAC || PLATFORM == PLATFORM_UNIX
	int nonBlocking = 1;
	if (fcntl(handle, F_SETFL, O_NONBLOCK, nonBlocking) == -1)
	{
		TSLogger::Log("Failed to set non-blocking\n");
		return false;
	}
#elif PLATFORM == PLATFORM_WINDOWS
	DWORD nonBlocking = 1;
	if (ioctlsocket(handle, FIONBIO, &nonBlocking) != 0)
	{
		TSLogger::Log("Failed to set non-blocking\n");
		return false;
	}
#endif

	return true;
}

void Socket::Close()
{
	// Destroy a socket
#if PLATFORM == PLATFORM_MAC || PLATFORM == PLATFORM_UNIX
	close(handle);
#elif PLATFORM == PLATFORM_WINDOWS
	closesocket(handle);
#endif
}

bool Socket::IsOpen() const
{
	TSLogger::Log("IsOpen function is unimplemented");
	return false;
}

bool Socket::Send(const Address& destination, const void* data, int size)
{
	// Creating destination address
	sockaddr_in destAddress;
	destAddress.sin_family = AF_INET;
	destAddress.sin_addr.s_addr = htonl(destination.GetAddress());
	destAddress.sin_port = htons(destination.GetPort());

	// Sending a packet to a specific address
	int sent_bytes = sendto(handle, (const char*)data, size, 0, (sockaddr*)&destAddress, sizeof(sockaddr_in));

	// Note that the return value only indicated whether the packet was sent successfully (not necessarily received)
	if (sent_bytes != size)
	{
		TSLogger::Log("Failed to send packet\n");
		return false;
	}
	return true;
}

int Socket::Receive(Address& sender, void* data, int size)
{

#if PLATFORM == PLATFORM_WINDOWS
	typedef int socklen_t;
#endif

	sockaddr_in from;
	socklen_t fromLength = sizeof(from);
	// Note that any packets larger than the max size are silently discarded!
	int bytes = recvfrom(handle, (char*)data, size, 0, (sockaddr*)&from, &fromLength);

	if (bytes <= 0)
	{
		return 0;
	}

	// Modify the sender's address and port
	sender.SetAddress(ntohl(from.sin_addr.s_addr));
	sender.SetNewPort(ntohs(from.sin_port));
	return bytes;
}
