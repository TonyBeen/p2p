/*************************************************************************
    > File Name: udpsocket.h
    > Author: hsz
    > Brief:
    > Created Time: Mon 14 Mar 2022 03:02:41 PM CST
 ************************************************************************/

#ifndef __EULAR_CORE_UDPSOCKET_H__
#define __EULAR_CORE_UDPSOCKET_H__

#include "address.h"
#include "socket.h"
#include "epoll.h"
#include "iomanager.h"
#include <utils/utils.h>
#include <memory>
#include <map>

namespace eular {

class UdpServer : public Socket
{
    DISALLOW_COPY_AND_ASSIGN(UdpServer);
public:
    typedef std::shared_ptr<UdpServer> SP;

    UdpServer(Epoll::SP epoll, IOManager *io_worker);
    ~UdpServer();

    void start();
    void stop();

    void onReadEvent();
    void onWriteEvent();
    void onTimerEvent();

protected:
    IOManager*  mIOWorker;
    std::map<String8, std::pair<Address, uint64_t>>  mUdpClientMap; // uuid, address, 上次发送数据的时间
    Mutex       mMutex;                     // 保证mUdpClientMap的增删不冲突
    uint32_t    mDisconnectionTimeoutMS;    // 超过此时间未发送数据意味着断开连接
    Epoll::SP   mEpoll;
};

} // namespace eular


#endif // __EULAR_CORE_UDPSOCKET_H__