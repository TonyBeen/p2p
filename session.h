/*************************************************************************
    > File Name: session.h
    > Author: hsz
    > Brief:
    > Created Time: 2022-03-31 17:06:34 Thursday
 ************************************************************************/

#ifndef __EULAR_P2P_SESSION_H__
#define __EULAR_P2P_SESSION_H__

#include <memory>

namespace eular {

class Session
{
public:
    typedef std::shared_ptr<Session> SP;

    Session();
    ~Session();

    virtual void onReadEvent(int fd) = 0;
    virtual void onWritEvent(int fd) = 0;
    virtual void onShutdown() = 0;

};

} // namespace eular

#endif // __EULAR_P2P_SESSION_H__
