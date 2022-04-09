/*************************************************************************
    > File Name: epoll.h
    > Author: hsz
    > Brief:
    > Created Time: 2022-03-28 10:33:35 Monday
 ************************************************************************/

#ifndef __EULAR_P2P_NET_EPOLL_H__
#define __EULAR_P2P_NET_EPOLL_H__

#include "session.h"
#include "iomanager.h"
#include "net/socket.h"
#include <sys/epoll.h>
#include <map>

namespace eular {

class Epoll
{
public:
    typedef std::shared_ptr<Epoll> SP;

    Epoll(IOManager *worker, IOManager *io_worker);
    ~Epoll();

    bool start();
    void stop();

    bool addEvent(Socket::SP clientSock, Session::SP session, uint32_t event = EPOLLIN | EPOLLOUT);
    bool addEvent(Socket::SP clientSock, std::function<void(int)> readCB,
        std::function<void(int)> writeCB, uint32_t event = EPOLLIN | EPOLLOUT);
    bool delEvent(Socket::SP clientSock, uint32_t event = EPOLLIN | EPOLLOUT);

private:
    void event_loop();
    // void contextVectorResize(int fd);
    // void clientVectorResize(int fd);
    void vectorResize(int fd);
    void removeFromEpoll(int fd);
    void resetFromContextVec(int fd);

    struct FDContext {
        typedef std::shared_ptr<FDContext> SP;
        int fd;
        uint32_t event;         // EPOLLIN | EPOLLOUT
        Session::SP session;    // 服务 优先级大于回调
        Mutex mutex;            // 防止在调用时被reset
        std::function<void(int)> callbackOfRead;
        std::function<void(int)> callbackOfWrite;

        FDContext() : fd(-1), event(0) {}
        FDContext(int f, uint32_t ev, std::function<void(int)> cbRead, std::function<void(int)> cbWrite = nullptr) :
            fd(f), event(ev), callbackOfRead(cbRead), callbackOfWrite(cbWrite) {}
        FDContext(Session::SP s) : session(s) {}
        ~FDContext()
        {
            reset();
        }

        void reset()
        {
            AutoLock<Mutex> lock(mutex);
            fd = -1;
            event = 0;
            callbackOfRead = nullptr;
            callbackOfWrite = nullptr;
            session.reset();
        }

        void executeEvent(uint32_t ev)
        {
            AutoLock<Mutex> lock(mutex);
            if (ev & EPOLLIN) {
                if (session != nullptr) {
                    session->onReadEvent(fd);
                } else {
                    callbackOfRead(fd);
                }
            }
            if (ev & EPOLLOUT) {
                if (session != nullptr) {
                    session->onWritEvent(fd);
                } else {
                    callbackOfWrite(fd);
                }
            }
        }
    };
    

private:
    int                         mEpollFd;
    IOManager*                  mWorker;       // 任务处理
    IOManager*                  mIOWorker;     // IO处理
    Mutex                       mMutex;        // 锁
    uint32_t                    mEventSize;    // epoll所能装下的最大数量
    std::atomic<bool>           mShouldStop;   // 是否停止
    std::vector<Socket::SP>     mClientVec;    // 已添加的event集合
    std::vector<FDContext::SP>  mContextVec;   // FDContext数组
};

} // namespace eular

#endif // __EULAR_P2P_NET_EPOLL_H__
