#pragma once
#include "../Posix/Address.h"
#include "NetworkCommon.h"

struct NetworkConfig
{
	AppID m_AppProtocolID{ 0 };
	Address m_ServerAddress{};
	float m_ConnectionTimeout{ 10.0f };
	float m_SyncPingFrequency{ 0.05f };
	float m_RequestConnectionFrequency{ 1.0f };
};

