/*************************************************************************
    > File Name: tcp_server.h
    > Author: hsz
    > Brief:
    > Created Time: Sat 26 Mar 2022 05:27:41 PM CST
 ************************************************************************/

#ifndef __EULAR_P2P_NET_TCP_SERVER_H__
#define __EULAR_P2P_NET_TCP_SERVER_H__

#include "socket.h"
#include "iomanager.h"
#include <atomic>

namespace eular {

class TcpServer : public Socket
{
public:
    TcpServer(IOManager *worker, IOManager *io_worker, IOManager *accept_worker);
    virtual ~TcpServer();

    virtual bool start();
    virtual void stop();

    bool bind(const Address &addr) override;
    bool listen(int backlog = SOMAXCONN) override;
    void close() override;

protected:
    virtual void start_accept();
    virtual void handle_client(Socket::SP client);

protected:
    enum class TcpStep {
        NOT_INIT = 0,   // 未初始化
        SOCKET = 1,     // 已调用socket
        BIND = 2,       // 已bind
        LISTEN = 3,     // 已listen, 可以accept
    };

protected:
    TcpStep     mStep;          // 流程
    uint64_t    mRecvTimeOut;   // 接收超时
    uint64_t    mSendTimeOut;   // 发送超时
    uint16_t    mKeepAliveTime; // 心跳检测
    IOManager  *mWorker;        // 处理一般的事件
    IOManager  *mIOWorker;      // 处理IO事件
    IOManager  *mAcceptWorker;  // 处理接收事件
    std::atomic<bool>   mStop;  // 是否需要停止
    friend class Epoll;
};

} // namespace eular

#endif // __EULAR_P2P_NET_TCP_SERVER_H__
