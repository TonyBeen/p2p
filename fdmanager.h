/*************************************************************************
    > File Name: fdmanager.h
    > Author: hsz
    > Brief: 管理套接字
    > Created Time: Mon 21 Mar 2022 04:39:47 PM CST
 ************************************************************************/

#ifndef __EULAR_FD_MANAGER_H__
#define __EULAR_FD_MANAGER_H__

#include <utils/utils.h>
#include <utils/singleton.h>
#include <utils/mutex.h>
#include <vector>
#include <memory>

namespace eular {
class Fdmanager;
class FdContext : public std::enable_shared_from_this<FdContext>
{
    DISALLOW_COPY_AND_ASSIGN(FdContext);
public:
    typedef std::shared_ptr<FdContext> SP;
    ~FdContext();

    bool isInit() const { return mIsInit; }
    bool isSocket() const { return mIsSocket; }
    bool isClosed() const { return mIsClosed; }
    
    void setUserNonblock(bool f) { mIsUserNonblock = f; }
    bool getUserNonoblock() const { return mIsUserNonblock; }
    void setSysNonblock(bool f) { mIsSysNonblock = f; }
    bool getSysNonblock() const { return mIsSysNonblock; }

    /**
     * @brief 设置超时时间
     * 
     * @param type SO_RCVTIMEO(读超时), SO_SNDTIMEO(写超时)
     * @param ms 
     */
    void setTimeOut(int type, uint64_t ms);
    uint64_t getTimeOut(int type) const; 

private:
    FdContext(int fd);
    
    bool init();
    friend class Fdmanager;

private:
    bool mIsInit : 1;           // 是否初始化
    bool mIsSocket : 1;         // 是否socket
    bool mIsSysNonblock : 1;    // 是否hook非阻塞
    bool mIsUserNonblock : 1;   // 是否主动设置为非阻塞
    bool mIsClosed : 1;         // 是否关闭

    int      mFd;               // 文件句柄
    uint64_t recvTimeout;       // 读超时时间
    uint64_t sendTimeout;       // 写超时时间
};

class Fdmanager
{
public:
    FdContext::SP get(int fd, bool needCreate = false);
    void del(int fd);

protected:
    Fdmanager();
    ~Fdmanager();

private:
    RWMutex     mRWMutex;
    std::vector<FdContext::SP>  mFdCtxVec;
    
    friend class Singleton<Fdmanager>;
};


typedef Singleton<Fdmanager> FdManager;

} // namespace eular

#endif // __EULAR_FD_MANAGER_H__
