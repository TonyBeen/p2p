/*************************************************************************
    > File Name: p2p_session.h
    > Author: hsz
    > Brief:
    > Created Time: 2022-03-31 16:54:04 Thursday
 ************************************************************************/

#ifndef __EULAR_P2P_P2P_SESSION_H__
#define __EULAR_P2P_P2P_SESSION_H__

#include "protocol/protocol.h"
#include "net/socket.h"
#include "session.h"
#include "util/uuid.h"

namespace eular {

/**
 * @brief peer连接后为其分配一个session，用于处理请求
 * 
 */
class P2PSession : public Session
{
public:
    typedef std::shared_ptr<P2PSession> SP;

    P2PSession(Socket::SP sock);
    ~P2PSession();

    virtual void onReadEvent(int fd) override;
    virtual void onWritEvent(int fd) override;
    virtual void onShutdown() override;

protected:
    void onRequestSendPeerInfo(const P2S_Request &req);
    void onRequestGetPeerInfo(const P2S_Request &req);
    void onRequestConnectToPeer(const P2S_Request &req);

protected:
    Socket::SP  mClientSocket;
    String8     mUUIDKey;
    UUID        mUuid;
    bool        mRefresh;   // 如果uuid不是第一次创建，则此值为true
};

} // namespace eular

#endif // __EULAR_P2P_P2P_SESSION_H__
