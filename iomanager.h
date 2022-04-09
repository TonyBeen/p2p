/*************************************************************************
    > File Name: iomanager.h
    > Author: hsz
    > Brief:
    > Created Time: Sat 05 Mar 2022 02:48:54 PM CST
 ************************************************************************/

#ifndef __EULAR_IOMANAGER_H__
#define __EULAR_IOMANAGER_H__

#include "fiber/scheduler.h"
#include "core/timer.h"
#include <utils/mutex.h>
#include <vector>

namespace eular {
class IOManager : public Scheduler, public TimerManager
{
public:
    IOManager(int threads, bool userCaller, String8 threadName);
    virtual ~IOManager();

    enum Event {
        NONE = 0,
        READ = EPOLLIN,
        WRITE = EPOLLOUT
    };

    int  addEvent(int fd, Event ev, std::function<void()> cb = nullptr);
    bool delEvent(int fd, Event ev);
    bool cancelEvent(int fd, Event ev);
    bool cancelAll(int fd);

    static IOManager *GetThis();

protected:
    virtual void idle() override;
    virtual void tickle() override;
    virtual bool stopping() override;
    virtual void onTimerInsertedAtFront() override;

    struct Context {
        struct EventContext {
            Scheduler *scheduler = nullptr;
            Fiber::SP fiber;
            std::function<void()> cb;
        };

        EventContext& getContext(Event event);

        // TODO: 将参数改成Event
        void resetContext(EventContext& ctx);
        void triggerEvent(Event event);

        EventContext read;
        EventContext write;
        int fd = 0;
        Event events = NONE;
        Mutex mutex;
    };

    void contextResize(uint32_t size);
    bool stopping(uint64_t& timeout);

private:
    int         mEpollFd;
    int         mTickleFd[2];       // pipe：用于触发epoll
    RWMutex     mRWMutex;           // 管理IOManager
    std::vector<Context *> mContextVec;
};

} // namespace eular

#endif