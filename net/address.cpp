/*************************************************************************
    > File Name: address.cpp
    > Author: hsz
    > Brief:
    > Created Time: Sun 13 Mar 2022 10:15:58 PM CST
 ************************************************************************/

#include "address.h"

namespace eular {

Address::Address(const String8 &ip, uint16_t port) :
    mIP(ip),
    mPort(port)
{

}

Address::Address(const sockaddr_in &addr)
{
    mIP = inet_ntoa(addr.sin_addr);
    mPort = ntohs(addr.sin_port);
}

Address::Address(const Address &other)
{
    mIP = other.mIP;
    mPort = other.mPort;
}

Address &Address::operator=(const Address& other)
{
    if (&other == this) {
        return *this;
    }

    mIP = other.mIP;
    mPort = other.mPort;
    return *this;
}

Address &Address::operator=(const sockaddr_in &addr)
{
    mIP = inet_ntoa(addr.sin_addr);
    mPort = ntohs(addr.sin_port);

    return *this;
}

Address::SP Address::CreateAddress(const String8 &ip, uint16_t port)
{
    return std::make_shared<Address>(ip, port);
}

Address::SP Address::CreateAddress(const sockaddr_in &addr)
{
    return std::make_shared<Address>(addr);
}

uint32_t Address::BigEndianIP(const String8 &ip)
{
    return inet_addr(ip.c_str());
}

uint16_t Address::BigEndianPort(uint16_t port)
{
    return htons(port);
}

sockaddr_in Address::getsockaddr() const
{
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = getBigEndianIP();
    addr.sin_port = getBigEndianPort();

    return addr;
}

void Address::dump(String8 &out) const
{
    out.appendFormat("[%s:%u]", mIP.c_str(), mPort);
}

String8 Address::dump() const
{
    return String8::format("[%s:%u]", mIP.c_str(), mPort);
}

} // namespace eular
