/*************************************************************************
    > File Name: p2p_service.h
    > Author: hsz
    > Brief:
    > Created Time: 2022-03-30 09:27:08 Wednesday
 ************************************************************************/

#ifndef __EULAR_P2P_SERVICE_H__
#define __EULAR_P2P_SERVICE_H__

#include "net/epoll.h"
#include "net/tcp_server.h"

namespace eular {

class P2PService : public TcpServer
{
public:
    typedef std::shared_ptr<P2PService> SP;
    P2PService(Epoll::SP epoll, IOManager *worker, IOManager *io_worker, IOManager *accept_worker);
    virtual ~P2PService();

    virtual bool start();
    virtual void stop();

protected:
    virtual void handle_client(Socket::SP client);
    Epoll::SP mEpoll;
};

} // namespace eular

#endif // __EULAR_P2P_SERVICE_H__
