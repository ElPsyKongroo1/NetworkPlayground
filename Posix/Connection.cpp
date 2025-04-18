#include "Connection.h"
#include "../Util/Logger.h"


ConnectionList::ConnectionList(ClientIndex maxClients) : m_MaxClients(maxClients)
{
	m_AllConnections.resize(maxClients);
	m_ClientsConnected.resize(maxClients);
}

ClientIndex ConnectionList::AddConnection(Address newAddress)
{
	// Ensure number of clients are not already at capacity
	if (m_NumClients >= m_MaxClients)
	{
		TSLogger::Log("Maximum number of connections reached");
		return k_InvalidClientIndex;
	}

	// Check for an empty connection slot
	ClientIndex iteration{ 0 };
	for (bool connected : m_ClientsConnected)
	{
		if (!connected)
		{
			// Reset the connection
			Connection& indicatedConnection = m_AllConnections[iteration];
			m_ClientsConnected[iteration] = true;
			indicatedConnection.m_Address = newAddress;
			indicatedConnection.m_ReliabilityContext = ReliabilityContext();

			// Update connection list state
			m_NumClients++;

			// Return index
			return iteration;
		}
		iteration++;
	}

	// Failed to find an empty connection slot
	TSLogger::Log("No connection slots are available");
	return k_InvalidClientIndex;
}

bool ConnectionList::RemoveConnection(ClientIndex clientIndex)
{
	// Check for out-of-bounds client
	if (clientIndex >= m_ClientsConnected.size())
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

	// Decriment the client count
	KG_ASSERT(m_NumClients > 0);
	m_NumClients--;

	return true;
}

Connection* ConnectionList::GetConnection(ClientIndex clientIndex)
{
	if (clientIndex >= m_AllConnections.size() || !m_ClientsConnected[clientIndex])
	{
		return nullptr;
	}
	return &m_AllConnections[clientIndex];
}

ClientIndex ConnectionList::GetNumberOfClients()
{
	return m_NumClients;
}

bool ConnectionList::IsConnectionActive(ClientIndex clientIndex)
{
	// Handle the defined null case
	if (clientIndex == k_InvalidClientIndex)
	{
		return false;
	}

	// Handle weird case where out of bounds client is provided
	if (clientIndex >= m_AllConnections.size())
	{
		TSLogger::Log("Attempt to query if a client is connected that is out of bounds %d", clientIndex);
		return false;
	}
	return m_ClientsConnected[clientIndex];
}

std::vector<Connection>& ConnectionList::GetAllConnections()
{
	return m_AllConnections;
}
