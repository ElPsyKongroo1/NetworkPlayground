#include "Address.h"

Address::Address()
{
	address = 0x7F000001; // 127.0.0.1
	this->port = 3000;
}

Address::Address(unsigned char a, unsigned char b, unsigned char c, unsigned char d, unsigned short port)
{
	SetAddress(a, b, c, d);
	this->port = port;
}

Address::Address(unsigned int address, unsigned short port)
{
	this->address = address;
	this->port = port;
}

unsigned int Address::GetAddress() const
{
	return address;
}

unsigned char Address::GetA() const
{
	return address >> 24;
}

unsigned char Address::GetB() const
{
	return (address >> 16) & 0xFF;
}

unsigned char Address::GetC() const
{
	return (address >> 8) & 0xFF;
}

unsigned char Address::GetD() const
{
	return address & 0xFF;
}

unsigned short Address::GetPort() const
{
	return port;
}

void Address::SetAddress(unsigned int newAddress)
{
	address = newAddress;
}

void Address::SetAddress(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
	address = (a << 24) | (b << 16) | (c << 8) | d;
}

void Address::SetNewPort(unsigned short newPort)
{
	port = newPort;
}

bool Address::operator==(const Address& other) const
{
	return (this->address == other.address) && (this->port == other.port);
}