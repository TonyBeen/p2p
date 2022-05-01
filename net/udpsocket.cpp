/*************************************************************************
    > File Name: udpsocket.cpp
    > Author: hsz
    > Brief:
    > Created Time: Mon 14 Mar 2022 03:02:32 PM CST
 ************************************************************************/

#include "udpsocket.h"
#include "config.h"
#include "fdmanager.h"
#include "db/redispool.h"
#include "protocol/protocol.h"
#include <log/log.h>

#define LOG_TAG "udpsocket"

namespace eular {
/**
 * udpserver的作用就是为了获得客户端的对外IP和port，加以保存
 */
UdpServer::UdpServer(Epoll::SP epoll, IOManager *io_worker, IOManager *processWorker) :
    Socket(SOCK_DGRAM),
    mIOWorker(io_worker),
    mProcessWorker(processWorker)
{
    newSock();

    LOG_ASSERT2(mSocket > 0);
    LOGD("udp socket %d", mSocket);
    mDisconnectionTimeoutMS = Config::Lookup<uint32_t>("udp.disconnection_timeout_ms", 3000);
    mEpoll = epoll;
    FdManager::get()->get(mSocket, true)->setUserNonblock(true);

    mMutex.setMutexName("udpserver");
}

UdpServer::~UdpServer()
{
    close();
}

void UdpServer::start()
{
    LOG_ASSERT2(mEpoll->addEvent(shared_from_this(), std::bind(&UdpServer::onReadEvent, this), nullptr, EPOLLIN));
    mTimerID = mProcessWorker->addTimer(1000, std::bind(&UdpServer::onTimerEvent, this), 1500);
    LOG_ASSERT2(mTimerID > 0);
}

void UdpServer::stop()
{
    mEpoll->delEvent(shared_from_this(), EPOLLIN);
    mProcessWorker->delTimer(mTimerID);
}

void UdpServer::onReadEvent()
{
    LOGD("UdpServer::onReadEvent()");
    P2S_Request req;
    Peer_Info info;
    Address addr;
    ProtocolParser parser;
    ByteBuffer ret;
    P2S_Response response;
    std::shared_ptr<RedisPool::RedisAPI> redis = RedisManager::get()->getRedis();
    response.statusCode = (uint16_t)P2PStatus::OK;
    strcpy(response.msg, Status2String(P2PStatus::OK).c_str());

    while (true) {
        ByteBuffer buffer;
        int readSize = Socket::recvfrom(buffer, addr);
        LOGD("UdpServer::onReadEvent() recv size %d", readSize);
        if (readSize <= 0) {
            break;
        }

        String8 log;
        for (int i = 0; i < buffer.size(); ++i) {
            if (i % 16 == 0) {
                log.appendFormat("\n\t");
            }
            log.appendFormat("0x%02x ", buffer[i]);
        }
        LOGD("%s() recv: %s", __func__, log.c_str());
        
        if (parser.parse(buffer) == false) {
            LOGW("%s() ProtocolParser error from [%s:%d]", __func__, addr.getIP().c_str(), addr.getPort());
            break;
        }

        ByteBuffer &data = parser.data();
        LOGD("%s() udp client [%s:%d] request flag 0x%04x", __func__,
            addr.getIP().c_str(), addr.getPort(), parser.commnd());
        switch (parser.commnd()) {
        case P2P_REQUEST_SEND_PEER_INFO:    // 客户端想要建立udp连接，此时对端发送的应该是tcp回复的uuid
            {
                memcpy(&info, data.const_data(), data.size());
                LOGD("uuid: %s", info.peer_uuid);
                // 插入数据到client map
                AutoLock<Mutex> lock(mMutex);
                mUdpClientMap[info.peer_uuid] = Time::Abstime();
            }
            {
                response.flag = P2P_RESPONSE_SEND_PEER_INFO;
                String8 uuid = info.peer_uuid;
                if (redis && redis->redisInterface()->isKeyExist(uuid)) { // 当键存在时
                    redis->redisInterface()->hashSetFiledValue(uuid, "udphost", addr.getIP());
                    redis->redisInterface()->hashSetFiledValue(uuid, "udpport", String8::format("%u", addr.getPort()));
                } else {
                    response.statusCode = (uint16_t)P2PStatus::NO_CONTENT;
                    strcpy(response.msg, Status2String(P2PStatus::NO_CONTENT).c_str());
                }
                ret = ProtocolGenerator::generator(P2P_RESPONSE_SEND_PEER_INFO, (uint8_t *)&response, P2S_Response_Size);
            }
            break;
        case P2P_REQUEST_HEARTBEAT_DETECT:
            {
                bool shouldResponse = false;
                {
                    memcpy(&info, data.const_data(), data.size());
                    LOGD("uuid: %s", info.peer_uuid);

                    if (redis) {    // 先从redis检查key是否还存在
                        if (redis->redisInterface()->isKeyExist(info.peer_uuid) == false) {
                            mUdpClientMap.erase(info.peer_uuid);
                        }
                    }
                    // 更新用户数据信息
                    AutoLock<Mutex> lock(mMutex);
                    const auto &it = mUdpClientMap.find(info.peer_uuid);
                    if (it != mUdpClientMap.end()) {
                        mUdpClientMap[info.peer_uuid] = Time::Abstime();
                        shouldResponse = true;
                    } else {
                        response.statusCode = (uint16_t)P2PStatus::NO_CONTENT;
                        strcpy(response.msg, Status2String(P2PStatus::NO_CONTENT).c_str());
                    }
                }
                if (shouldResponse) {
                    response.flag = P2P_RESPONSE_HEARTBEAT_DETECT;
                    String8 uuid = info.peer_uuid;
                    if (redis) {
                        redis->redisInterface()->hashSetFiledValue(uuid, "udphost", addr.getIP());
                        redis->redisInterface()->hashSetFiledValue(uuid, "udpport",
                            String8::format("%u", addr.getPort()));
                    }
                    ret = ProtocolGenerator::generator(P2P_RESPONSE_HEARTBEAT_DETECT, (uint8_t *)&response, P2S_Response_Size);
                }
            }
            break;
        default:
            break;
        }

        LOGD("%s() send buf size = %zu", __func__, ret.size());
        log.clear();
        for (int i = 0; i < ret.size(); ++i) {
            if (i % 16 == 0) {
                log.appendFormat("\n\t");
            }
            log.appendFormat("0x%02x ", ret[i]);
        }
        LOGD("%s() send: %s", __func__, log.c_str());
        Socket::sendto(ret, addr);
        ret.clear();
    }
}

void UdpServer::onTimerEvent()
{
    AutoLock<Mutex> lock(mMutex);
    uint64_t currentTimeMS = Time::Abstime();
    for (auto it = mUdpClientMap.begin(); it != mUdpClientMap.end();) {
        if (it->second < (currentTimeMS - mDisconnectionTimeoutMS)) {    // 当超过3s未收到数据则认为其断开连接
            auto redis = RedisManager::get()->getRedis();
            if (redis && redis->redisInterface()->isKeyExist(it->first)) {
                static const char *fields[] = { "udphost", "udpport" };
                redis->redisInterface()->hashDelFileds(it->first, fields, 2);
            } else {
                LOGW("getRedis return null or uuid: \'%s\' is not exist!", it->first.c_str());
            }
            mUdpClientMap.erase(it++);
            continue;
        }
        ++it;
    }
}

} // namespace eular

