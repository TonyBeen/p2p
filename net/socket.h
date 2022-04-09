/*************************************************************************
    > File Name: socket.h
    > Author: hsz
    > Brief:
    > Created Time: Fri 25 Mar 2022 10:04:36 AM CST
 ************************************************************************/

#ifndef __EULAR_NET_SOCKET_H__
#define __EULAR_NET_SOCKET_H__

#include "address.h"
#include <utils/utils.h>
#include <utils/Buffer.h>
#include <utils/string8.h>
#include <stdint.h>
#include <memory>

namespace eular {

class Socket : public std::enable_shared_from_this<Socket>
{
    DISALLOW_COPY_AND_ASSIGN(Socket);
public:
    typedef std::shared_ptr<Socket> SP;
    typedef std::weak_ptr<Socket> WP;

    static Socket::SP CreateTCP(const Address &addr);
    static Socket::SP CreateUDP(const Address &addr);

    Socket(int type);
    virtual ~Socket();

    bool cancelRead();
    bool cancelWrite();
    bool cancelAccept();
    bool cancelAll();

    bool settimeout(uint16_t type, uint64_t timeoutMS);
    int64_t gettimeout(uint16_t type);

    template<class T>
    bool setOption(int level, int option, const T& value)
    {
        return setOption(level, option, &value, sizeof(T));
    }

    template<class T>
    bool getOption(int level, int option, T& result)
    {
        socklen_t length = sizeof(T);
        return getOption(level, option, &result, &length);
    }

    virtual bool bind(const Address &addr);
    virtual bool listen(int backlog = SOMAXCONN);
    virtual bool connect(const Address &addr, uint64_t timeout = 0);
    virtual bool reconnect(uint64_t timeout = 0);
    virtual Socket::SP accept();
    virtual void close();

    virtual int send(const void *buf, size_t len, int flag = 0);
    virtual int send(const iovec *buffer, size_t len, int flag = 0);
    virtual int send(const ByteBuffer &buffer, int flag = 0);
    virtual int sendto(const void *buf, size_t len, const Address &to, int flag = 0);
    virtual int sendto(const iovec *buf, size_t len, const Address &to, int flag = 0);
    virtual int sendto(const ByteBuffer &buf, const Address &to, int flag = 0);

    virtual int recv(void *buf, size_t len, int flag = 0);
    virtual int recv(iovec *buffer, size_t len, int flag = 0);
    virtual int recv(ByteBuffer &buffer, int flag = 0);
    virtual int recvfrom(void *buf, size_t len, Address &from, int flag = 0);
    virtual int recvfrom(iovec *buffer, size_t len, Address &from, int flag = 0);
    virtual int recvfrom(ByteBuffer &buffer, Address &from, int flag = 0);

    Address::SP getLocalAddr();
    Address::SP getRemoteAddr();

    bool valid() const { return mSocket != -1; }
    int socket() const { return mSocket; }
    int type() const { return mType; }
    bool isConnected() const { return mIsConnected; }
    String8 dump() const;

protected:
    virtual bool init(int sock);
    void initSock();
    void newSock();
    bool setOption(int level, int option, const void* result, socklen_t len);
    bool getOption(int level, int option, void* result, socklen_t* len);

protected:
    int         mSocket;
    Address::SP mLocalAddr;
    Address::SP mRemoteAddr;
    int         mType;    // SOCK_STREAM || SOCK_DGRAM
    const int   mFamily;
    bool        mIsConnected;
};

} // namespace eular

#endif // __EULAR_NET_SOCKET_H__
