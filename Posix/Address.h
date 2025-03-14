#pragma once
#include <xhash>

class Address
{
public:
	Address();
	Address(unsigned char a, unsigned char b, unsigned char c, unsigned char d, unsigned short port);
	Address(unsigned int address, unsigned short port);

	unsigned int GetAddress() const;

	unsigned char GetA() const;
	unsigned char GetB() const;
	unsigned char GetC() const;
	unsigned char GetD() const;
	unsigned short GetPort() const;

	void SetAddress(unsigned int newAddress);
	void SetAddress(unsigned char a, unsigned char b, unsigned char c, unsigned char d);
	void SetNewPort(unsigned short newPort);

	bool operator==(const Address& other) const;

private:
	unsigned int address{ 0 };
	unsigned short port{ 0 };
};

namespace std 
{
	template <>
	struct hash<Address> 
	{
		size_t operator()(const Address& addr) const 
		{
			// Combine the hash of address and port
			size_t addrHash = std::hash<unsigned int>()(addr.GetAddress());
			size_t portHash = std::hash<unsigned short>()(addr.GetPort());
			return addrHash ^ (portHash << 1); // Combine the two hash values
		}
	};
}