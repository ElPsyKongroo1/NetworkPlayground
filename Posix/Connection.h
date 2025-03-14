#pragma once


#include "Address.h"

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <limits>

struct Connection
{
	Address m_Address;
	float m_LastPacketReceived{ 0.0f };
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