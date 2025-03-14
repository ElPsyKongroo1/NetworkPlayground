#include "Connection.h"
#include "../Util/Logger.h"


Connection* ConnectionMap::AddConnection(Address newAddress)
{
	Connection newConnection;
	newConnection.m_Address = newAddress;
	newConnection.m_LastPacketReceived = 0.0f;

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
