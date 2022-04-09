/*************************************************************************
    > File Name: address.h
    > Author: hsz
    > Brief:
    > Created Time: Sun 13 Mar 2022 10:15:54 PM CST
 ************************************************************************/

#ifndef __EULAR_CORE_ADDRESS_H__
#define __EULAR_CORE_ADDRESS_H__

#include <utils/string8.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <memory>

namespace eular {
class Address
{
public:
    typedef std::shared_ptr<Address> SP;

    Address() { mPort = 0; }
    Address(const String8 &ip, uint16_t port);
    Address(const sockaddr_in &sockaddr);
    Address(const Address &other);
    ~Address() {}

    Address &operator=(const Address& addr);
    Address &operator=(const sockaddr_in &addr);

    static SP CreateAddress(const String8 &ip, uint16_t port);
    static SP CreateAddress(const sockaddr_in &addr);

    void setIP(const String8 &ip) { mIP = ip; }
    void setPort(uint16_t port) { mPort = port; }

    const String8 &getIP() const { return mIP; }
    const uint16_t &getPort() const { return mPort; }
    uint32_t getBigEndianIP() const { return inet_addr(mIP.c_str()); }
    uint16_t getBigEndianPort() const { return htons(mPort); }

    static uint32_t BigEndianIP(const String8 &ip);
    static uint16_t BigEndianPort(uint16_t port);

    sockaddr_in getsockaddr() const;

    void dump(String8 &out) const;
    String8 dump() const;

protected:
    String8     mIP;
    uint16_t    mPort;
};

} // namespace eular

#endif // __EULAR_CORE_ADDRESS_H__
