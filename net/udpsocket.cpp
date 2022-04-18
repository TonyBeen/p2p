/*************************************************************************
    > File Name: udpsocket.cpp
    > Author: hsz
    > Brief:
    > Created Time: Mon 14 Mar 2022 03:02:32 PM CST
 ************************************************************************/

#include "udpsocket.h"
#include "config.h"
#include "db/redispool.h"
#include "protocol/protocol.h"
#include <log/log.h>

#define LOG_TAG "udpsocket"

namespace eular {
/**
 * udpserver的作用就是为了获得客户端的对外IP和port，加以保存
 */
UdpServer::UdpServer(Epoll::SP epoll, IOManager *io_worker) :
    Socket(SOCK_DGRAM),
    mIOWorker(io_worker)
{
    newSock();
    LOG_ASSERT2(mSocket > 0);
    mDisconnectionTimeoutMS = Config::Lookup<uint32_t>("udp.disconnection_timeout_ms", 3000);
    mEpoll = epoll;
}

UdpServer::~UdpServer()
{
    close();
}

void UdpServer::start()
{
    mEpoll->addEvent(shared_from_this(), std::bind(&UdpServer::onReadEvent, this), nullptr, EPOLLIN);
    mIOWorker->addTimer(1000, std::bind(&UdpServer::onTimerEvent, this), 1500);
}

void UdpServer::stop()
{
    mEpoll->delEvent(shared_from_this(), EPOLLIN);
}

void UdpServer::onReadEvent()
{
    P2S_Request req;
    Address addr;

    std::shared_ptr<RedisPool::RedisAPI> redis;
    int tryTimes = 10;
    do {
        redis = RedisManager::get()->getRedis();
        --tryTimes;
    } while (redis == nullptr && tryTimes > 0); // TODO: 循环多少次合适

    while (true) {
        int readSize = recvfrom(&req, sizeof(P2S_Request), addr, 0);
        if (readSize < 0) {
            if (errno != EAGAIN) {
                LOGE("%s() reacvfrom error. [%d, %s]", __func__, errno, strerror(errno));
            }
            break;
        }
        if (readSize != sizeof(P2S_Request)) {
            LOGW("read size != sizeof(P2S_Request");
            continue;
        }

        LOGD("%s() request flag 0x%04x", req.flag);
        switch (req.flag) {
        case P2P_REQUEST_SEND_PEER_INFO:    // 客户端想要建立udp连接，此时对端发送的应该是tcp回复的uuid
            LOGI("%s() udp client [%s:%d] uuid: \"%s\"", __func__,
                addr.getIP().c_str(), addr.getPort(), req.peer_info.peer_uuid);
            {
                // 插入数据到client map
                AutoLock<Mutex> lock(mMutex);
                mUdpClientMap[req.peer_info.peer_uuid] = std::make_pair(addr, Time::Abstime());
            }
            {
                String8 uuid = req.peer_info.peer_uuid;
                if (redis && redis->redisInterface()->isKeyExist(uuid)) { // 当键存在时
                    redis->redisInterface()->hashSetFiledValue(uuid, "udphost", addr.getIP());
                    redis->redisInterface()->hashSetFiledValue(uuid, "udpport",
                        String8::format("%u", addr.getPort()));
                }
            }
            break;
        case P2P_REQUEST_HEARTBEAT_DETECT:
            LOGD("%s() udp client [%s:%d] uuid: \"%s\"", __func__,
                addr.getIP().c_str(), addr.getPort(), req.peer_info.peer_uuid);
            {
                // 更新用户数据信息
                AutoLock<Mutex> lock(mMutex);
                const auto &it = mUdpClientMap.find(req.peer_info.peer_uuid);
                if (it != mUdpClientMap.end()) {
                    mUdpClientMap[req.peer_info.peer_uuid] = std::make_pair(addr, Time::Abstime());
                }
            }
            {
                String8 uuid = req.peer_info.peer_uuid;
                if (redis) {
                    redis->redisInterface()->hashSetFiledValue(uuid, "udphost", addr.getIP());
                    redis->redisInterface()->hashSetFiledValue(uuid, "udpport",
                        String8::format("%u", addr.getPort()));
                }
            }
            break;
        default:
            break;
        }
    }
}

void UdpServer::onTimerEvent()
{
    AutoLock<Mutex> lock(mMutex);
    uint64_t currentTimeMS = Time::Abstime();
    for (auto it = mUdpClientMap.begin(); it != mUdpClientMap.end(); ++it) {
        if (it->second.second < (currentTimeMS - mDisconnectionTimeoutMS)) {    // 当超过3s未收到数据则认为其断开连接
            mUdpClientMap.erase(it++);
            auto redis = RedisManager::get()->getRedis();
            if (redis) {
                static const char *fields[] = { "udphost", "udpport" };
                redis->redisInterface()->hashDelFileds(it->first, fields, 2);
            } else {
                LOGW("getRedis return null. uuid: \'%s\' is not remove", it->first.c_str());
            }
        }
    }
}

} // namespace eular

