#pragma once
#include "../Posix/Address.h"
#include <cstdint>

using AppID = uint8_t;

enum class PacketType : uint8_t
{
	KeepAlive,
	Message,
	ConnectionRequest,
	ConnectionSuccess,
	ConnectionDenied
};

constexpr size_t k_ReliabilitySegmentSize
{ 
	sizeof(uint16_t) /*packetSequenceNum*/ + 
	sizeof(uint16_t) /*ackSequenceNum*/ +
	sizeof(uint32_t) /*ackBitfield*/
};
constexpr size_t k_PacketHeaderSize
{ 
	sizeof(AppID) /*appID*/ + 
	k_ReliabilitySegmentSize /*packetAckSegment*/ +
	sizeof(PacketType) /*packetType*/ 
};
constexpr size_t k_MaxPacketSize{ 256 };
constexpr size_t k_MaxPayloadSize{ k_MaxPacketSize - k_PacketHeaderSize };


struct NetworkConfig
{
	AppID m_AppProtocolID{ 201 };
	Address m_ServerAddress{};
	float m_ConnectionTimeout{ 10.0f };
	float m_SyncPingFrequency{ 0.05f };
	float m_RequestConnectionFrequency{ 5.0f };
};