/*************************************************************************
    > File Name: p2p_service.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2022-03-30 09:27:13 Wednesday
 ************************************************************************/

#include "p2p_service.h"
#include "p2p_session.h"
#include <log/log.h>

#define LOG_TAG "P2PService"

namespace eular {

P2PService::P2PService(Epoll::SP epoll, IOManager *worker, IOManager *io_worker, IOManager *accept_worker) :
    TcpServer(worker, io_worker, accept_worker)
{
    mEpoll = epoll;
    LOG_ASSERT2(mEpoll != nullptr);
}

P2PService::~P2PService()
{

}

bool P2PService::start()
{
    bool ret = TcpServer::start();
    ret &= mEpoll->start();
    return ret;
}

void P2PService::stop()
{
    TcpServer::stop();
    mEpoll->stop();
}

void P2PService::handle_client(Socket::SP client)
{
    LOGI("P2PService::handle_client()");
    const Address::SP &addr = client->getRemoteAddr();
    LOGI("processing client %d %s:%u", client->socket(), addr->getIP().c_str(), addr->getPort());

    P2PSession::SP session(new P2PSession(client));

    if (mEpoll->addEvent(client, session, EPOLLIN) != true) {
        // LOGE("%s() addEvent error. [%d, %s]", __func__, errno, strerror(errno));
    }
}


} // namespace eular
