/*************************************************************************
    > File Name: service.h
    > Author: hsz
    > Brief:
    > Created Time: 2022-03-31 15:18:50 Thursday
 ************************************************************************/

#ifndef __EULAR_P2P_NET_SERVICE_H__
#define __EULAR_P2P_NET_SERVICE_H__

#include <memory>

namespace eular {

class Service
{
public:
    typedef std::shared_ptr<Service> SP;

    Service();
    ~Service();

    virtual void onReadEvent(int fd) = 0;
    virtual void onWritEvent(int fd) = 0;

protected:

};

} // namespace eular

#endif // __EULAR_P2P_NET_SERVICE_H__
