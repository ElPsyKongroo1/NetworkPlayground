#include "Connection.h"
#include "../Util/Logger.h"

#include <cmath>
#include <chrono>

#include <bit>
#include <iostream>
#include <format>

static float GetCurrentTime()
{
	using namespace std::chrono;
	return duration<float>(steady_clock::now().time_since_epoch()).count();
}

Connection* ConnectionMap::AddConnection(Address newAddress)
{
	Connection newConnection;
	newConnection.m_Address = newAddress;
	newConnection.m_ReliabilityContext.ResetLastPacketReceived();

	auto [connectIter, inserted] = m_Map.insert_or_assign(newAddress, newConnection);

	// Alert when a connection is assigned instead of inserted
	if (!inserted)
	{
		TSLogger::Log("New connection added was assigned instead of inserted.");
	}

	return &(m_Map.at(newAddress));
}


bool ConnectionMap::RemoveConnection(Address queryAddress)
{
	size_t numErased = m_Map.erase(queryAddress);
	return numErased > 0;
}

Connection* ConnectionMap::GetConnection(Address queryAddress)
{
	if (!m_Map.contains(queryAddress))
	{
		return nullptr;
	}

	return &(m_Map.at(queryAddress));
}

bool ConnectionMap::IsConnectionActive(Address queryAddress)
{
	return m_Map.contains(queryAddress);
}

std::unordered_map<Address, Connection>& ConnectionMap::GetMap()
{
	return m_Map;
}

void ReliabilityContext::InsertReliabilitySegmentIntoPacket(uint8_t* segmentLocation)
{
	// Initialize the locations of the seq, ack, and ack-bitfield
	uint16_t& sequenceLocation = *(uint16_t*)segmentLocation;
	uint16_t& ackLocation = *(uint16_t*)(segmentLocation + sizeof(sequenceLocation));
	uint32_t& bitFieldLocation = *(uint32_t*)(segmentLocation + 
		sizeof(sequenceLocation) + sizeof(ackLocation));

	//TSLogger::Log("Sent the packet %d. It has the ack %d. It also has the bit field: ", m_LocalSequence, m_RemoteSequence);
	//std::cout << std::format("{:032b}\n", m_RemoteAckField.GetRawBitfield());

	// Insert the sequence number (appID|[sequenceNum]|ackNum|ackBitField|...)
	InsertLocalSequenceNumber(sequenceLocation);

	// Insert the recent ack number (appID|sequenceNum|[ackNum]|ackBitField|...) 
	InsertRemoteSequenceNumber(ackLocation);

	// Insert the recent ack's bitfield (appID|sequenceNum|ackNum|[ackBitField]|...)
	InsertRemoteSequenceBitField(bitFieldLocation);
}

void ReliabilityContext::ProcessReliabilitySegmentFromPacket(uint8_t* segmentLocation)
{
	// Initialize the locations of the seq, ack, and ack-bitfield
	uint16_t packetSequence = *(uint16_t*)segmentLocation;
	uint16_t packetAck = *(uint16_t*)(segmentLocation + sizeof(packetSequence));
	uint32_t packetAckBitfield = *(uint32_t*)(segmentLocation +
		sizeof(packetSequence) + sizeof(packetAck));

	// Update the remote data based on the received sequence number
	if (!ProcessReceivedSequenceNumber(packetSequence))
	{
		return;
	}

	// Check the ack context
	if (!ProcessReceivedAck(packetAck, packetAckBitfield))
	{
		return;
	}

	// Packet received successfully
	m_LastPacketReceived = 0.0f;
}

void ReliabilityContext::InsertLocalSequenceNumber(uint16_t& sequenceLocation)
{
	// Insert the current local sequence number the into packet location
	sequenceLocation = m_LocalSequence;

	// Check for drop packet at the 32 bit boundary of the bitfield!
	if (!m_LocalAckField.IsFlagSet(31))
	{
		uint16_t droppedPacketSeq = sequenceLocation - 31;
		float packetRTT = GetCurrentTime() - GetTimePoint(droppedPacketSeq);
		UpdateAverageRoundTrip(packetRTT);
		//TSLogger::Log("===================================\n");
		//TSLogger::Log("Dropped a packet %d\n", droppedPacketSeq);
		//TSLogger::Log("-----------------------------------\n");
		//TSLogger::Log("Round trip time %fms\n", packetRTT * 1000.0f);
		//TSLogger::Log("Average round trip %fms\n", m_AverageRoundTrip * 1000.0f);
		//TSLogger::Log("===================================\n");
	}

	// Add new packet creation time to round trip calculator
	AddTimePoint(sequenceLocation);

	// Move sequence number to next packet number
	m_LocalSequence++;

	// Update the local bitfield to make space for the new packet
	m_LocalAckField.SetRawBitfield(m_LocalAckField.GetRawBitfield() << 1); // I want it to overflow!
}

void ReliabilityContext::InsertRemoteSequenceNumber(uint16_t& ackLocation)
{
	// Insert the current remote sequence number into the packet
	ackLocation = m_RemoteSequence;
}

void ReliabilityContext::InsertRemoteSequenceBitField(uint32_t& bitFieldLocation)
{
	// Insert the bitfield
	bitFieldLocation = (uint32_t)m_RemoteAckField.GetRawBitfield(); // Truncate the bitfield
}

bool ReliabilityContext::ProcessReceivedSequenceNumber(uint16_t receivedSequenceNumber)
{
	uint16_t distance = std::abs((int16_t)receivedSequenceNumber - (int16_t)m_RemoteSequence);
	bool excessiveDistance = distance > 31;

	// Check if the current sequence number is newer
	if (SequenceGreaterThan(receivedSequenceNumber, m_RemoteSequence))
	{
		// The received packet is 'newer' than the current sequence number's packet

		// Clear the bit set if we are shifting too much
		if (excessiveDistance)
		{
			m_RemoteAckField.ClearAllFlags();
			return true;
		}

		m_RemoteAckField.SetRawBitfield(m_RemoteAckField.GetRawBitfield() << (distance));
		m_RemoteAckField.SetFlag(0);
		

		// Update to the new sequence number
		m_RemoteSequence = receivedSequenceNumber;
	}
	else
	{

		// The received packet is 'older' than the current sequence number's packet
		
		// Early out if we are shifting too much or packet is already ack'd
		if (excessiveDistance || m_RemoteAckField.IsFlagSet((uint8_t)distance))
		{
			return false;
		}

		// Update the bit of the packet received
		m_RemoteAckField.SetFlag((uint8_t)distance);
	}

	return true;
}

bool ReliabilityContext::ProcessReceivedAck(uint16_t ackNumber, uint32_t ackBitField)
{
	uint16_t distance = std::abs((int16_t)m_LocalSequence - (int16_t)ackNumber);
	bool excessiveDistance = distance > 32;

	// The ack number should never be greater than the local sequence value
	// (If so, likely a corrupted packet or a bad actor)
	if (SequenceGreaterThan(ackNumber, m_LocalSequence) || 
		ackNumber == m_LocalSequence || 
		excessiveDistance)
	{
		return false;
	}
	//TSLogger::Log("==========================================\n");
	//TSLogger::Log("[Local context before receive ack] Local Sequence: %d. Bitfield start: %d. Bitfield: ", m_LocalSequence, m_LocalSequence - 1);
	//std::cout << std::format("{:032b}\n", m_LocalAckField.GetRawBitfield());
	//TSLogger::Log("[Packet context before receive ack] Packet Sequence & Start: %d. Bitfield: ", ackNumber);
	//std::cout << std::format("{:032b}\n", ackBitField);
	//TSLogger::Log("------------------------------------------\n");

	// Modify the received bit field to align with the local bitfield
	ackBitField = (ackBitField << (distance - 1));

	//TSLogger::Log("Packet bitfield after modification: ");
	//std::cout << std::format("{:032b}\n", ackBitField);

	// Use logical implication to reveal modified packets
	uint32_t newlyAcknowledgedField = (~m_LocalAckField.GetRawBitfield()) & ackBitField;

	//TSLogger::Log("Newly acknowledged bitfield: ");
	//std::cout << std::format("{:032b}\n", newlyAcknowledgedField);

	// Scan the newly-acknowledged-field and acknowledge the packets
	// TODO: Add meaningful acknowledgement method here
	for (uint16_t iteration{ 0 }; iteration < 32; iteration++)
	{
		// Check this specific bit
		bool isSet = (newlyAcknowledgedField >> iteration) & 1;

		// Acknowledged a packet
		if (isSet)
		{
			uint16_t ackPacketSeq = m_LocalSequence - 1 - iteration;
			float packetRTT = GetCurrentTime() - GetTimePoint(ackPacketSeq);
			UpdateAverageRoundTrip(packetRTT);

			/*TSLogger::Log("===================================\n");
			TSLogger::Log("Acknowledged a packet %d\n", ackPacketSeq);
			TSLogger::Log("-----------------------------------\n");
			TSLogger::Log("Round trip time %fms\n", packetRTT * 1000.0f);
			TSLogger::Log("Average round trip %fms\n", m_AverageRoundTrip * 1000.0f);
			TSLogger::Log("===================================\n");*/
		}
	}

	// Finally, update the local bitfield with new acknowledgements
	m_LocalAckField.SetRawBitfield(m_LocalAckField.GetRawBitfield() | ackBitField);

	return newlyAcknowledgedField;

	//TSLogger::Log("------------------------------------------\n");
	//TSLogger::Log("[Local context before after ack] Local Sequence: %d. Bitfield start: %d. Bitfield: ", m_LocalSequence, m_LocalSequence - 1);
	//std::cout << std::format("{:032b}\n", m_LocalAckField.GetRawBitfield());
	//TSLogger::Log("[Packet context after receive ack] Packet Sequence & Start: %d. Bitfield: ", ackNumber);
	//std::cout << std::format("{:032b}\n", ackBitField);
	//TSLogger::Log("==========================================\n\n");
}

bool ReliabilityContext::SequenceGreaterThan(uint16_t sequence1, uint16_t sequence2)
{
	constexpr uint16_t k_HalfShort{ 32768 };

	return ((sequence1 > sequence2) && (sequence1 - sequence2 <= k_HalfShort)) ||
		((sequence1 < sequence2) && (sequence2 - sequence1 > k_HalfShort));
}

void ReliabilityContext::AddTimePoint(uint16_t sequenceNumber)
{
	m_SendTimepoints[sequenceNumber % 32] = GetCurrentTime();
}

float ReliabilityContext::GetTimePoint(uint16_t sequenceNumber)
{
	return m_SendTimepoints[sequenceNumber % 32];
}

void ReliabilityContext::UpdateAverageRoundTrip(float packetRoundTrip)
{
	constexpr float k_ShiftFactor{ 0.1f };

	m_AverageRoundTrip = (1.0f - k_ShiftFactor) * m_AverageRoundTrip + k_ShiftFactor * packetRoundTrip;

	// Update congestion values
	if (m_AverageRoundTrip > m_CongestionConfig.m_CongestedRTTThreshold)
	{
		m_TimeNotCongested = 0.0f;

		if (!m_IsCongested)
		{
			TSLogger::Log("Connection is now congested\n");
			m_IsCongested = true;
		}
	}
}

void ReliabilityContext::UpdateTimeInCongestionState(float deltaTime)
{
	if (m_AverageRoundTrip < m_CongestionConfig.m_CongestedRTTThreshold)
	{
		m_TimeNotCongested += deltaTime;
	}

	if (m_IsCongested && m_TimeNotCongested > m_ResetCongestedTime)
	{
		TSLogger::Log("Connection is no longer congested\n");
		m_IsCongested = false;
	}
}

void ReliabilityContext::ResetLastPacketReceived()
{
	m_LastPacketReceived = 0.0f;
}

void ReliabilityContext::UpdateLastPacketReceived(float deltaTime)
{
	m_LastPacketReceived += deltaTime;
}

float ReliabilityContext::GetLastPacketReceived()
{
	return m_LastPacketReceived;
}

bool ReliabilityContext::IsCongested()
{
	return m_IsCongested;
}

ConnectionList::ConnectionList(size_t maxClients) : m_MaxClients(maxClients)
{
	m_AllConnections.resize(maxClients);
	m_ClientsConnected.resize(maxClients);
}

std::tuple<Connection*, size_t> ConnectionList::CreateConnection(Address newAddress)
{
	// Check for an empty connection slot
	size_t iteration{ 0 };
	for (bool connected : m_ClientsConnected)
	{
		if (!connected)
		{
			// Reset the connection and return it
			Connection& indicatedConnection = m_AllConnections[iteration];
			indicatedConnection.m_Address = newAddress;
			indicatedConnection.m_ReliabilityContext = ReliabilityContext();
			return { &indicatedConnection, iteration };
		}
		iteration++;
	}

	// Failed to find an empty connection slot
	return { nullptr, k_InvalidClientIndex };
}

bool ConnectionList::RemoveConnection(size_t clientIndex)
{
	// Check for out-of-bounds client
	if (clientIndex >= m_MaxClients)
	{
		TSLogger::Log("Attempt to remove a client index that is out of bounds %d", clientIndex);
		return false;
	}

	// Check for already disconnected client
	if (!m_ClientsConnected[clientIndex])
	{
		TSLogger::Log("Attempt to remove a client that is already disconnected %d", clientIndex);
		return false;
	}

	// Remove the client
	m_ClientsConnected[clientIndex] = false;
	m_AllConnections[]

	return true;
}
