#pragma once


#include "Address.h"
#include "../Util/BitField.h"

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <limits>
#include <array>
#include <optional>
#include <tuple>

struct CongestionConfig
{
	float m_CongestedRTTThreshold{ 250.0f / 1000.0f }; // Quarter of a second round-trip
	float m_DefaultResetCongestedTimeSec{ 10.0f }; // 10 seconds
	float m_MaxResetCongestedTimeSec{ 60.0f }; // 60 seconds
	float m_MinResetCongestedTimeSec{ 1.0f }; // 1 second
};

class ReliabilityContext
{
public:
	void InsertReliabilitySegmentIntoPacket(uint8_t* segmentLocation);
	void ProcessReliabilitySegmentFromPacket(uint8_t* segmentLocation);

	void ResetLastPacketReceived();
	void UpdateLastPacketReceived(float deltaTime);
	float GetLastPacketReceived();

	bool IsCongested();

	void UpdateTimeInCongestionState(float deltaTime);
private:
	void InsertLocalSequenceNumber(uint16_t& sequenceLocation);
	void InsertRemoteSequenceNumber(uint16_t& ackLocation);
	void InsertRemoteSequenceBitField(uint32_t& bitFieldLocation);

	bool ProcessReceivedSequenceNumber(uint16_t receivedSequenceNumber);
	bool ProcessReceivedAck(uint16_t ackNumber, uint32_t ackBitField);

	bool SequenceGreaterThan(uint16_t sequence1, uint16_t sequence2);

	void AddTimePoint(uint16_t sequenceNumber);
	float GetTimePoint(uint16_t sequenceNumber);
	void UpdateAverageRoundTrip(float packetRoundTrip);

private:
	// Sequencing data
	uint16_t m_LocalSequence{0};
	uint16_t m_RemoteSequence{0};
	BitField<uint32_t> m_LocalAckField	{ 0b1111'1111'1111'1111'1111'1111'1111'1111 };
	BitField<uint32_t> m_RemoteAckField	{ 0b1111'1111'1111'1111'1111'1111'1111'1110 };

	// Time data
	float m_LastPacketReceived{ 0.0f };
	float m_AverageRoundTrip{ 0.0f };
	std::array<float, 32> m_SendTimepoints;

	// Congestion avoidance data
	bool m_IsCongested{ false };
	CongestionConfig m_CongestionConfig{};
	float m_TimeNotCongested{ 0.0f };
	float m_ResetCongestedTime{ 10.0f };
};

struct Connection
{
	Address m_Address;
	ReliabilityContext m_ReliabilityContext{};
};

class ConnectionMap
{
public:
	Connection* AddConnection(Address newAddress);
	bool RemoveConnection(Address queryAddress);

	Connection* GetConnection(Address queryAddress);
	bool IsConnectionActive(Address queryAddress);
	std::unordered_map<Address, Connection>& GetMap();
private:
	std::unordered_map<Address, Connection> m_Map;
};

constexpr size_t k_InvalidClientIndex{ std::numeric_limits<size_t>::max() };

class ConnectionList
{
public:
	ConnectionList(size_t maxClients);
public:
	std::tuple<Connection*, size_t> CreateConnection(Address newAddress);
	bool RemoveConnection(size_t clientIndex);

	Connection* GetConnection(size_t clientIndex);
	bool IsConnectionActive(size_t clientIndex);
private:
	size_t m_MaxClients{ 0 };
	std::vector<Connection> m_AllConnections{};
	std::vector<bool> m_ClientsConnected{};
};