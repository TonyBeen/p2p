/*************************************************************************
    > File Name: socket.cpp
    > Author: hsz
    > Brief:
    > Created Time: Fri 25 Mar 2022 10:04:41 AM CST
 ************************************************************************/

#include "socket.h"
#include "iomanager.h"
#include "fdmanager.h"
#include "hook.h"
#include <log/log.h>

#define LOG_TAG "socket"
#define INVALID_SOCKET -1

namespace eular {

Socket::Socket(int type) :
    mSocket(INVALID_SOCKET),
    mLocalAddr(nullptr),
    mRemoteAddr(nullptr),
    mType(type),
    mFamily(AF_INET),
    mIsConnected(false)
{
    LOGD("%s(%s)", __func__, type == SOCK_STREAM ? "SOCK_STREAM" : "SOCK_DGRAM");
}

Socket::~Socket()
{
    close();
}

bool Socket::bind(const Address &addr)
{
    if (!valid()) {
        newSock();
        if (eular_unlikely(!valid())) {
            return false;
        }
    }

    sockaddr_in sock_addr = addr.getsockaddr();
    if (::bind(mSocket, (sockaddr *)&sock_addr, sizeof(sockaddr_in))) {
        LOGE("%s() bind sock %d type %d error. [%d,%s]", __func__, mSocket, mType, errno, strerror(errno));
        return false;
    }
    return true;
}

bool Socket::listen(int backlog)
{
    if (!valid()) {
        LOGE("listen error. socket = -1");
        return false;
    }

    if (::listen(mSocket, backlog)) {
        LOGE("listen socket %d error. [%d,%s]", mSocket, errno, strerror(errno));
        return false;
    }
    return true;
}

bool Socket::connect(const Address &addr, uint64_t timeout)
{
    if (!valid()) {
        return false;
    }
    mRemoteAddr.reset(new Address(addr));

    sockaddr_in sock_addr = addr.getsockaddr();
    if (timeout == 0) {
        if (::connect(mSocket, (sockaddr *)&sock_addr, sizeof(sockaddr_in))) {
            LOGE("connect to %s:%d error. [%d,%s]", addr.getIP().c_str(), addr.getPort(), errno, strerror(errno));
            return false;
        }
    } else {
        if (::connect_with_timeout(mSocket, (sockaddr *)&sock_addr, sizeof(sockaddr_in), timeout)) {
            LOGE("%d connect to %s:%d timeout.", mSocket, addr.getIP().c_str(), addr.getPort());
            return false;
        }
    }
    mIsConnected = true;
    getLocalAddr();
    return true;
}

bool Socket::reconnect(uint64_t timeout)
{
    if (!mRemoteAddr) {
        return false;
    }
    mLocalAddr.reset();
    return connect(*mRemoteAddr, timeout);
}

Socket::SP Socket::accept()
{
    if (mSocket < 0) {
        return nullptr;
    }

    Socket::SP client(new Socket(mType));
    int client_sock = ::accept(mSocket, nullptr, nullptr);
    if (client_sock == -1) {
        LOGE("accept error. [%d,%s]", errno, strerror(errno));
        return nullptr;
    }

    if (!client->init(client_sock)) {
        return nullptr;
    }
    const auto &addr = client->getRemoteAddr();
    LOG_ASSERT2(addr != nullptr);
    LOGD("%s() %d client %s:%d connected.", __func__, client_sock,
        addr->getIP().c_str(), addr->getPort());
    return client;
}

void Socket::close()
{
    if (!mIsConnected && mSocket == INVALID_SOCKET) {
        return;
    }
    mIsConnected = false;
    if (mSocket != INVALID_SOCKET) {
        ::close(mSocket);
        mSocket = INVALID_SOCKET;
    }
}

int Socket::send(const void *buf, size_t len, int flag)
{
    if (mIsConnected) {
        return ::send(mSocket, buf, len ,flag);
    }

    LOGW("%s() sock %d not connected.", __func__, mSocket);
    return -1;
}

int Socket::send(const iovec *buffer, size_t len, int flag)
{
    if (mIsConnected) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec *)buffer;
        msg.msg_iovlen = len;
        return ::sendmsg(mSocket, &msg, flag);
    }

    LOGW("%s() sock %d not connected.", __func__, mSocket);
    return -1;
}

int Socket::send(const ByteBuffer &buffer, int flag)
{
    if (mIsConnected) {
        return ::send(mSocket, buffer.const_data(), buffer.size(), flag);
    }

    LOGW("%s() sock %d not connected.", __func__, mSocket);
    return -1;
}

int Socket::sendto(const void *buf, size_t len, const Address &to, int flag)
{
    if (mIsConnected) {
        sockaddr_in sock_addr = to.getsockaddr();
        return ::sendto(mSocket, buf, len, flag, (sockaddr *)&sock_addr, sizeof(sockaddr_in));
    }

    LOGW("%s() sock %d not connected.", __func__, mSocket);
    return -1;
}

int Socket::sendto(const iovec *buf, size_t len, const Address &to, int flag)
{
    if (mIsConnected) {
        sockaddr_in sock_addr = to.getsockaddr();
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buf;
        msg.msg_iovlen = len;
        msg.msg_name = (sockaddr *)&sock_addr;
        msg.msg_namelen = sizeof(sockaddr_in);
        return ::sendmsg(mSocket, &msg, flag);
    }

    LOGW("%s() sock %d not connected.", __func__, mSocket);
    return -1;
}

int Socket::sendto(const ByteBuffer &buf, const Address &to, int flag)
{
    sockaddr_in sock_addr = to.getsockaddr();
    if (mIsConnected) {
        return ::sendto(mSocket, buf.const_data(), buf.size(), flag, (sockaddr *)&sock_addr, sizeof(sockaddr_in));
    }

    LOGW("%s() sock %d not connected.", __func__, mSocket);
    return -1;
}

int Socket::recv(void *buf, size_t len, int flag)
{
    if (mIsConnected) {
        return ::recv(mSocket, buf, len, flag);
    }

    return -1;
}

int Socket::recv(iovec *buffer, size_t len, int flag)
{
    if (mIsConnected) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = buffer;
        msg.msg_iovlen = len;
        return ::recvmsg(mSocket, &msg, flag);
    }

    return -1;
}

int Socket::recv(ByteBuffer &buffer, int flag)
{
    if (mIsConnected) {
        int sum = 0;
        uint8_t buf[512] = {0};
        while (true) {
            int size = recv(buf, sizeof(buf), flag);
            if (size <= 0) {
                if (errno != EAGAIN) {
                    LOGE("recvfrom error. [%d,%s]", errno, strerror(errno));
                }
                break;
            }
            sum += size;
            buffer.append(buf, size);
        }
        return sum;
    }

    return -1;
}

int Socket::recvfrom(void *buf, size_t len, Address &from, int flag)
{
    if (mIsConnected) {
        sockaddr_in sock_addr;
        socklen_t socklen = sizeof(sockaddr_in);
        int size = ::recvfrom(mSocket, buf, len, flag, (sockaddr *)&sock_addr, &socklen);
        if (size > 0) {
            from = sock_addr;
        }
        return size;
    }

    return -1;
}

int Socket::recvfrom(iovec *buffer, size_t len, Address &from, int flag)
{
    if(mIsConnected) {
        sockaddr_in addr;
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = buffer;
        msg.msg_iovlen = len;
        msg.msg_name = (sockaddr *)&addr;
        msg.msg_namelen = sizeof(sockaddr_in);
        int size = ::recvmsg(mSocket, &msg, flag);
        if (size > 0) {
            from = addr;
        }
        return size;
    }
    return -1;
}

int Socket::recvfrom(ByteBuffer &buffer, Address &from, int flag)
{
    if (mIsConnected) {
        int sum = 0;
        uint8_t buf[512] = {0};
        sockaddr_in addr;
        socklen_t socklen = sizeof(sockaddr_in);
        while (true) {
            int size = ::recvfrom(mSocket, buf, sizeof(buf), flag, (sockaddr *)&addr, &socklen);
            if (size <= 0) {
                if (errno != EAGAIN) {
                    LOGE("recvfrom error. [%d,%s]", errno, strerror(errno));
                }
                break;
            }
            sum += size;
            buffer.append(buf, size);
        }

        from = addr;
        return sum;
    }

    return -1;
}

Address::SP Socket::getLocalAddr()
{
    if (mLocalAddr != nullptr) {
        return mLocalAddr;
    }

    if (valid()) {
        sockaddr_in addr;
        socklen_t len = sizeof(sockaddr_in);
        if (getsockname(mSocket, (sockaddr *)&addr, &len)) {
            LOGE("%s() getsockname error. [%d,%s]", __func__, errno, strerror(errno));
            return nullptr;
        }
        mLocalAddr.reset(new Address(addr));
    }

    return mLocalAddr;
}

Address::SP Socket::getRemoteAddr()
{
    if (mRemoteAddr != nullptr) {
        return mRemoteAddr;
    }
    
    if (valid()) {
        sockaddr_in addr;
        socklen_t len = sizeof(sockaddr_in);
        if (getpeername(mSocket, (sockaddr *)&addr, &len)) {
            LOGE("%s() getpeername error. [%d,%s]", __func__, errno, strerror(errno));
            return nullptr;
        }
        mRemoteAddr.reset(new Address(addr));
    }

    return mRemoteAddr;
}

String8 Socket::dump() const
{
    return String8::format("socket: %d; type 0x%x; connected: %d; local address: %s:%d; remote address: %s:%d;",
        mSocket, mType, mIsConnected,
        mLocalAddr->getIP().c_str(), mLocalAddr->getPort(),
        mRemoteAddr->getIP().c_str(), mRemoteAddr->getPort());
}

Socket::SP Socket::CreateTCP(const Address &addr)
{
    Socket::SP ptr(new Socket(SOCK_STREAM));
    return ptr;
}

Socket::SP Socket::CreateUDP(const Address &addr)
{
    Socket::SP ptr(new Socket(SOCK_DGRAM));
    return ptr;
}

bool Socket::cancelRead()
{
    auto *iom = IOManager::GetThis();
    LOG_ASSERT2(iom);
    return iom->cancelEvent(mSocket, IOManager::READ);
}

bool Socket::cancelWrite()
{
    auto *iom = IOManager::GetThis();
    LOG_ASSERT2(iom);
    return iom->cancelEvent(mSocket, IOManager::WRITE);
}

bool Socket::cancelAccept()
{
    auto *iom = IOManager::GetThis();
    LOG_ASSERT2(iom);
    return iom->cancelEvent(mSocket, IOManager::READ);
}

bool Socket::cancelAll()
{
    auto *iom = IOManager::GetThis();
    LOG_ASSERT2(iom);
    return iom->cancelAll(mSocket);
}

bool Socket::settimeout(uint16_t type, uint64_t timeoutMS)
{
    if (mSocket == INVALID_SOCKET) {
        return false;
    }
    timeval tv;
    tv.tv_sec = timeoutMS / 1000;
    tv.tv_usec = timeoutMS % 1000 * 1000;

    return setOption(SOL_SOCKET, type, &tv, sizeof(timeval));
}

int64_t Socket::gettimeout(uint16_t type)
{
    if (mSocket == INVALID_SOCKET) {
        return false;
    }
    timeval tv = {0, 0};
    socklen_t len = sizeof(timeval);
    if (getOption(SOL_SOCKET, type, &tv, &len)) {
        return tv.tv_sec * 1000 + tv.tv_usec / 1000;
    }
    return -1;
}

bool Socket::init(int sock)
{
    auto ctx = FdManager::get()->get(sock);
    if (ctx && ctx->isSocket() && !ctx->isClosed()) {
        mSocket = sock;
        mIsConnected = true;
        initSock();
        getLocalAddr();
        getRemoteAddr();
        return true;
    }
    return false;
}

void Socket::initSock()
{
    int val = 1;
    setOption(SOL_SOCKET, SO_REUSEADDR, val);
    if(mType == SOCK_STREAM) {
        setOption(IPPROTO_TCP, TCP_NODELAY, val);
    }
}

void Socket::newSock()
{
    mSocket = ::socket(mFamily, mType, 0);
    if(eular_unlikely(mSocket != -1)) {
        initSock();
    } else {
        LOGE("%S() socket error. [%d,%s]", __func__, errno, strerror(errno));
    }
}

bool Socket::setOption(int level, int option, const void* result, socklen_t len)
{
    if (setsockopt(mSocket, level, option, result, len)) {
        LOGE("%s() setsockopt error. sock %d, level %d, option %d. [%d,%s]",
            __func__, mSocket, level, option, errno, strerror(errno));
        return false;
    }
    return true;
}

bool Socket::getOption(int level, int option, void* result, socklen_t* len)
{
    if (getsockopt(mSocket, level, option, result, len)) {
        LOGE("%s() getsockopt error. sock %d, level %d, option %d. [%d,%s]",
            __func__, mSocket, level, option, errno, strerror(errno));
        return false;
    }
    return true;
}

   
} // namespace eular
