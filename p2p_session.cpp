/*************************************************************************
    > File Name: p2p_session.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2022-03-31 16:54:09 Thursday
 ************************************************************************/

#include "p2p_session.h"
#include "db/redispool.h"
#include <utils/Buffer.h>
#include <utils/mutex.h>
#include <log/log.h>

#define LOG_TAG "P2PSession"

namespace eular {

P2PSession::P2PSession(Socket::SP sock) :
    mRefresh(false)
{
    mClientSocket.swap(sock);
}

P2PSession::~P2PSession()
{

}

void P2PSession::onReadEvent(int fd)
{
    P2P_Request req;
    P2P_Response response;

    std::vector<Peer_Info> peerInfoVec;
    std::shared_ptr<RedisPool::RedisAPI> redis;
    int tryTimes = 10;
    do {
        redis = RedisManager::get()->getRedis();
        --tryTimes;
    } while (redis == nullptr && tryTimes > 0); // TODO: 循环多少次合适

    while (true) {
        memset(&response, 0, sizeof(P2P_Response));
        memset(&req, 0, sizeof(P2P_Request));
        response.statusCode = (uint16_t)P2PStatus::OK;
        strcpy(response.msg, Status2String(P2PStatus::OK).c_str());

        int recvSize = mClientSocket->recv(&req, P2P_Request_Size);
        if (recvSize <= 0) {
            if (errno != EAGAIN) {
                LOGE("%s() recv error. [%d, %s]", __func__, errno, strerror(errno));
            }
            break;
        }

        const Address::SP &addr = mClientSocket->getRemoteAddr();
        if (recvSize != P2P_Request_Size) {
            LOGW("client %d [%s:%u] send invalid request", fd, addr->getIP().c_str(), addr->getPort());
            continue;
        }

        LOGD("%s() client %d [%s:%u] send request 0x%04x", __func__, fd, addr->getIP().c_str(), addr->getPort(), req.flag);
        switch (req.flag) {
        case P2P_REQUEST_SEND_PEER_INFO:    // 客户端发送本机信息
            {
                response.flag = P2P_RESPONSE_SEND_PEER_INFO;
                String8 name = req.peer_info.peer_name;
                name.appendFormat("+%s", addr->getIP().c_str());
                if (mRefresh && redis != nullptr) {
                    redis->redisInterface()->delKey(mUuid.uuid());
                }
                mUuid.init(name);
                mRefresh = true;
                LOGD("client %d uuid: %s", fd, mUuid.uuid().c_str());
                std::vector<std::pair<String8, String8>> fields;
                fields.push_back(std::make_pair("name", name));
                fields.push_back(std::make_pair("tcphost", addr->getIP()));
                fields.push_back(std::make_pair("tcpport", String8::format("%u", addr->getPort())));

                if (redis != nullptr) {
                    if (redis->redisInterface()->hashCreateOrReplace(mUuid.uuid(), fields)) {
                        response.statusCode = (uint16_t)P2PStatus::REDIS_SERVER_ERROR;  // redis错误
                        strcpy(response.msg, Status2String(P2PStatus::REDIS_SERVER_ERROR).c_str());
                    }
                }
                response.number = 0;
            }
            break;
        case P2P_REQUEST_GET_PEER_INFO:
            {
                response.flag = P2P_RESPONSE_GET_PEER_INFO;
                std::vector<String8> uuidVec;
                if (redis && redis->redisInterface()->getAllKeys(uuidVec)) { // 获取数据所有的键，数据库存错的键的类型都是哈希键
                    response.statusCode = (uint16_t)P2PStatus::REDIS_SERVER_ERROR;
                    strcpy(response.msg, Status2String(P2PStatus::REDIS_SERVER_ERROR).c_str());
                }
                std::map<String8, String8> fieldVal;
                for (auto &uuid : uuidVec) {
                    if (redis && redis->redisInterface()->hashGetKeyAll(uuid, fieldVal) > 0) {
                        Peer_Info info;
                        auto name = fieldVal.find("name");
                        auto udpIP = fieldVal.find("udphost");
                        auto udpPort = fieldVal.find("udpport");
                        if (name == fieldVal.end() ||
                            udpIP == fieldVal.end() ||
                            udpPort == fieldVal.end()) {
                            continue;
                        }
                        info.host_binary = inet_addr(udpIP->second.c_str());
                        uint32_t port;
                        sscanf(udpPort->second.c_str(), "%u", &port);
                        info.port_binary = htons(port);
                        strcpy(info.peer_name, name->second.c_str());
                        strcpy(info.peer_uuid, uuid.c_str());
                        peerInfoVec.push_back(info);
                    }
                }
                response.number = peerInfoVec.size();
            }
            break;
        case P2P_REQUEST_CONNECT_TO_PEER:
            {
                // TODO: 将对端想要连接的uuid相关信息从redis拿出来，并告知此客户端有人想要与其建立连接
                // 由于无法拿到另一端的tcp socket导致无法发送数据，此条作废。信息即已给出，则由客户端自己进行管理
                response.flag = P2P_RESPONSE_CONNECT_TO_PEER;
                response.number = 0;
            }
            break;
        default:
            LOGW("unknow flag 0x%04x", req.flag);
            break;
        }

        mClientSocket->send(&response, sizeof(P2P_Response));
        for (auto &info : peerInfoVec) {
            mClientSocket->send(&info, sizeof(Peer_Info));
        }
        peerInfoVec.clear();
    }
}

void P2PSession::onWritEvent(int fd)
{
    LOGD("%s()", __func__);
}

void P2PSession::onRequestSendPeerInfo(const P2P_Request &req)
{

}

void P2PSession::onRequestGetPeerInfo(const P2P_Request &req)
{

}

void P2PSession::onRequestConnectToPeer(const P2P_Request &req)
{

}

} // namespace eular
