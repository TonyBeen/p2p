/*************************************************************************
    > File Name: tcp_server.cpp
    > Author: hsz
    > Brief:
    > Created Time: Sat 26 Mar 2022 05:27:44 PM CST
 ************************************************************************/

#include "tcp_server.h"
#include "config.h"
#include <log/log.h>

#define LOG_TAG "TcpServer"

namespace eular {

TcpServer::TcpServer(IOManager *worker, IOManager *io_worker, IOManager *accept_worker) :
    Socket(SOCK_STREAM),
    mWorker(worker),
    mIOWorker(io_worker),
    mAcceptWorker(accept_worker),
    mStop(true)
{
    newSock();
    LOG_ASSERT2(mSocket > 0);
    mStep = TcpStep::SOCKET;
    mRecvTimeOut = Config::Lookup<uint64_t>("tcp.recv_timeout", 1000);
    mSendTimeOut = Config::Lookup<uint64_t>("tcp.send_timeout", 2000);
    mKeepAliveTime = Config::Lookup<uint16_t>("tcp.keep_alive_time", 30);
}

TcpServer::~TcpServer()
{
    stop();
}

bool TcpServer::start()
{
    if (!mStop) {
        return true;
    }
    if (mStep != TcpStep::LISTEN) {
        return false;
    }
    mStop = false;
    mAcceptWorker->schedule(std::bind(&TcpServer::start_accept, this));
    return true;
}

void TcpServer::stop()
{
    if (mStop) {
        return;
    }
    mStop = true;
    mAcceptWorker->schedule([this](){
        cancelAll();
        close();
    });
}

bool TcpServer::bind(const Address &addr)
{
    LOG_ASSERT2(mStep == TcpStep::SOCKET);
    sockaddr_in sock_addr = addr.getsockaddr();
    if (0 != ::bind(mSocket, (sockaddr *)&sock_addr, sizeof(sockaddr_in))) {
        LOGE("bind error. [%d,%s]", errno, strerror(errno));
        return false;
    }
    mStep = TcpStep::BIND;
    return true;
}

bool TcpServer::listen(int backlog)
{
    if (mStep != TcpStep::BIND) {
        return false;
    }
    if (0 != ::listen(mSocket, backlog)) {
        LOGE("listen error. [%d,%s]", errno, strerror(errno));
        return false;
    }

    mStep = TcpStep::LISTEN;
    return true;
}

void TcpServer::close()
{
    if (mSocket > 0) {
        ::close(mSocket);
        mSocket = -1;
    }
}

void TcpServer::start_accept()
{
    while (!mStop) {
        Socket::SP client = accept();
        if (client != nullptr) {
            client->settimeout(SO_RCVTIMEO, mRecvTimeOut);
            client->settimeout(SO_SNDTIMEO, mSendTimeOut);
            client->setOption<int>(SOL_SOCKET, SO_KEEPALIVE, mKeepAliveTime);
            mIOWorker->schedule(std::bind(&TcpServer::handle_client, this, client));
        }
    }
}

void TcpServer::handle_client(Socket::SP client)
{
    LOGI("TcpServer::%s()", __func__);
}

} // namespace eular
