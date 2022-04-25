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
    LOGD("%s(%d)", __func__, fd);
    P2S_Request req;
    P2S_Response response;
    ByteBuffer buffer;
    ProtocolParser parser;

    std::vector<Peer_Info> peerInfoVec;
    std::shared_ptr<RedisPool::RedisAPI> redis = RedisManager::get()->getRedis();

    while (true) {
        memset(&response, 0, sizeof(P2S_Response));
        memset(&req, 0, sizeof(P2S_Request));
        response.statusCode = (uint16_t)P2PStatus::OK;
        strcpy(response.msg, Status2String(P2PStatus::OK).c_str());

        int recvSize = mClientSocket->recv(buffer);
        LOGD("%s() %d recv size %d", __func__, fd, recvSize);
        if (recvSize <= 0) {
            if (errno != EAGAIN) {
                LOGE("%s() recv error. [%d, %s]", __func__, errno, strerror(errno));
            }
            break;
        }

        // FIXME: 当多个数据包到达时ProtocolParser只能解析出一个，之后的无法解析出
        if (parser.parse(buffer) == false) {
            break;
        }

        ByteBuffer &data = parser.data();
        if (data.size() != P2S_Request_Size) {
            LOGW("recv an invalid request. buffer size %zu", data.size());
        }

        const Address::SP &addr = mClientSocket->getRemoteAddr();
        LOGD("%s() client %d [%s:%u] send request 0x%04x", __func__, fd, addr->getIP().c_str(), addr->getPort(), req.flag);
        switch (parser.commnd()) {
        case P2P_REQUEST_SEND_PEER_INFO:    // 客户端发送本机信息
            {
                // 本条命令接待的数据应该是Peer_Info
                Peer_Info info;
                memcpy(&info, data.const_data(), data.size());
                response.flag = P2P_RESPONSE_SEND_PEER_INFO;
                String8 name = info.peer_name;
                mUUIDKey = String8::format("%s+%s", name.c_str(), addr->getIP().c_str());
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
                response.number = 1;
                strcpy(info.peer_uuid, mUuid.uuid().c_str());
                peerInfoVec.push_back(info);
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
                    if (uuid == mUuid.uuid()) { // 排除自身
                        continue;
                    }
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
                // 通过拿到的udp信息告知其有客户端想要连接
                response.flag = P2P_RESPONSE_CONNECT_TO_PEER;
                response.number = 0;
            }
            break;
        default:
            LOGW("unknow flag 0x%04x", req.flag);
            break;
        }

        ByteBuffer temp;
        temp.append((uint8_t *)&response, sizeof(P2S_Response));
        mClientSocket->send(&response, sizeof(P2S_Response));
        temp.append((uint8_t *)&peerInfoVec[0], sizeof(Peer_Info) * peerInfoVec.size());
        ByteBuffer retsult = ProtocolGenerator::generator(P2P_RESPONSE, temp);
        mClientSocket->send(retsult);

        peerInfoVec.clear();
    }
}

void P2PSession::onWritEvent(int fd)
{
    LOGD("%s()", __func__);
}

void P2PSession::onRequestSendPeerInfo(const P2S_Request &req)
{

}

void P2PSession::onRequestGetPeerInfo(const P2S_Request &req)
{

}

void P2PSession::onRequestConnectToPeer(const P2S_Request &req)
{

}

} // namespace eular
