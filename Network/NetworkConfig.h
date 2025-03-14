#pragma once
#include "../Posix/Address.h"
#include <cstdint>

using AppID = uint32_t;

enum class PacketType : uint8_t
{
	KeepAlive,
	Message,
	ConnectionRequest,
	ConnectionSuccess,
	ConnectionDenied
};

constexpr size_t k_MaxPayloadSize{ 256 };
constexpr size_t k_MaxPacketSize{ sizeof(AppID) /*appID*/ + sizeof(PacketType) /*packetType*/ + k_MaxPayloadSize /*payload*/};


struct NetworkConfig
{
	AppID m_AppProtocolID{ 12345 };
	Address m_ServerAddress{};
	float m_ConnectionTimeout{ 10.0f };
	float m_SyncPingFrequency{ 1.0f };
	float m_RequestConnectionFrequency{ 5.0f };
};