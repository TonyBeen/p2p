/*************************************************************************
    > File Name: iomanager.cpp
    > Author: hsz
    > Brief:
    > Created Time: Sat 05 Mar 2022 02:48:58 PM CST
 ************************************************************************/

#include "iomanager.h"
#include <utils/Errors.h>
#include <utils/exception.h>
#include <log/log.h>
#include <sys/epoll.h>

#define LOG_TAG "IOManager"
#define EPOLL_MAX_SIZE 4096
#define TOSTR(something) #something

namespace eular {

IOManager::IOManager(int threads, bool userCaller, String8 threadName) :
    Scheduler(threads, userCaller, threadName)
{
    LOGI("%s() start", __func__);
    mEpollFd = epoll_create(EPOLL_MAX_SIZE);
    LOG_ASSERT(mEpollFd > 0, "epoll_create error. [%d,%s]", errno, strerror(errno));

    int ret = pipe(mTickleFd);
    LOG_ASSERT(!ret, "pipe error. [%d,%s]", errno, strerror(errno));

    epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = mTickleFd[0];

    ret = fcntl(mTickleFd[0], F_SETFL, O_NONBLOCK);
    LOG_ASSERT2(!ret);

    ret = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mTickleFd[0], &event);
    LOG_ASSERT(!ret, "epoll_ctl(EPOLL_CTL_ADD) error. [%d,%s]", errno, strerror(errno));

    contextResize(256);
    start();
}

IOManager::~IOManager()
{
    stop();
    close(mEpollFd);
    close(mTickleFd[0]);
    close(mTickleFd[1]);

    for (size_t i = 0; i < mContextVec.size(); ++i) {
        if (mContextVec[i]) {
            delete mContextVec[i];
        }
    }
}

int IOManager::addEvent(int fd, IOManager::Event ev, std::function<void()> cb)
{
    Context *ctx = nullptr;

    mRWMutex.rlock();
    if (fd < mContextVec.size()) {
        ctx = mContextVec[fd];
    } else {
        mRWMutex.unlock();
        mRWMutex.wlock();
        contextResize(fd * 1.5);
        ctx = mContextVec[fd];
    }
    mRWMutex.unlock();
    if (eular_unlikely(ctx->events & ev)) { // 已存在此事件
        LOGW("%s() %d[ev: 0x%x] already exists", __func__, fd, ev);
        return INVALID_PARAM;
    }

    uint32_t opt = ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event event;
    event.events = EPOLLET | ctx->events | ev;
    event.data.ptr = ctx;
    int ret = epoll_ctl(mEpollFd, opt, fd, &event);
    if (ret) {
        LOGE("epoll_ctl(%d, %s, %d, 0x%x) error. [%d,%s]", mEpollFd, opt == EPOLL_CTL_MOD ? TOSTR(EPOLL_CTL_MOD) : TOSTR(EPOLL_CTL_ADD),
            fd, event.events, errno, strerror(errno));
        return UNKNOWN_ERROR;
    }

    ctx->events = (IOManager::Event)(ctx->events | ev);
    Context::EventContext &eventCtx = ctx->getContext(ev);
    LOG_ASSERT2(!eventCtx.scheduler && !eventCtx.fiber && !eventCtx.cb);
    eventCtx.scheduler = Scheduler::GetThis();
    if (cb) {
        eventCtx.cb.swap(cb);
    } else {
        eventCtx.fiber = Fiber::GetThis();
        LOG_ASSERT2(eventCtx.fiber->getState() == Fiber::EXEC);
    }

    return OK;
}

bool IOManager::delEvent(int fd, IOManager::Event ev)
{
    Context *ctx = nullptr;
    {
        RDAutoLock<RWMutex> rdlock(mRWMutex);
        if (mContextVec.size() <= fd) {
            return false;
        }
        Context *ctx = mContextVec[fd];
        LOG_ASSERT2(ctx != nullptr);
    }

    AutoLock<Mutex> lock(ctx->mutex);
    if (eular_unlikely(!(ctx->events & ev))) {
        LOGW("%s() invalid param event 0x%x", __func__, (uint32_t)ev);
        return false;
    }

    Event newEvent = (Event)(ctx->events & ~ev);
    int opt = newEvent ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.data.ptr = ctx;
    epevent.events = EPOLLET | newEvent;
    int ret = epoll_ctl(mEpollFd, opt, fd, &epevent);
    if (ret) {
        LOGE("epoll_ctl(%d, %s, %d, 0x%x) error. [%d,%s]", mEpollFd,
            opt == EPOLL_CTL_MOD ? TOSTR(EPOLL_CTL_MOD) : TOSTR(EPOLL_CTL_DEL),
            fd, epevent.events, errno, strerror(errno));
        return false;
    }

    ctx->events = newEvent;
    Context::EventContext &eventCtx = ctx->getContext(ev);
    ctx->resetContext(eventCtx);
    return true;
}

bool IOManager::cancelEvent(int fd, IOManager::Event ev)
{
    Context *ctx = nullptr;
    {
        RDAutoLock<RWMutex> rdlock(mRWMutex);
        if (mContextVec.size() <= fd) {
            return false;
        }
        ctx = mContextVec[fd];
        LOG_ASSERT2(ctx != nullptr);
    }

    AutoLock<Mutex> lock(ctx->mutex);
    if (eular_unlikely(!(ctx->events & ev))) {
        return false;
    }
    Event newEv = (Event)(ctx->events & ~ev);
    int opt = newEv ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.data.ptr = ctx;
    epevent.events = EPOLLET | newEv;

    int ret = epoll_ctl(mEpollFd, opt, fd, &epevent);
    if (ret < 0) {
        LOGE("epoll_ctl(%d, %s, %d, 0x%x) error. [%d,%s]", mEpollFd,
            opt == EPOLL_CTL_MOD ? TOSTR(EPOLL_CTL_MOD) : TOSTR(EPOLL_CTL_DEL),
            fd, epevent.events, errno, strerror(errno));
        return false;
    }
    ctx->triggerEvent(ev);
    return true;
}

bool IOManager::cancelAll(int fd)
{
    Context *ctx = nullptr;
    {
        RDAutoLock<RWMutex> rdlock(mRWMutex);
        if (mContextVec.size() <= fd) {
            return false;
        }
        ctx = mContextVec[fd];
        LOG_ASSERT2(ctx != nullptr);
    }

    AutoLock<Mutex> lock(ctx->mutex);
    int ret = epoll_ctl(mEpollFd, EPOLL_CTL_DEL, fd, nullptr);
    if (ret < 0) {
        LOGE("epoll_ctl(%d, EPOLL_CTL_DEL, %d, nullptr) error. [%d,%s]", mEpollFd,
            fd, errno, strerror(errno));
        return false;
    }

    if (ctx->events & IOManager::READ) {
        ctx->triggerEvent(IOManager::READ);
    } else if (ctx->events & IOManager::WRITE) {
        ctx->triggerEvent(IOManager::WRITE);
    }

    LOG_ASSERT2(ctx->events == IOManager::NONE);
    return true;
}

IOManager *IOManager::GetThis()
{
    return dynamic_cast<IOManager *>(Scheduler::GetThis());
}

void IOManager::idle()
{
    LOGD("IOManager::idle()");
    const uint32_t maxEvents = 256;
    epoll_event *events = new epoll_event[maxEvents]();
    std::shared_ptr<epoll_event> ptr(events, [](epoll_event *p) {
        if (p) {
            delete[] p;
        }
    });

    while (true) {
        uint64_t nextTimeout = 0;
        if (eular_unlikely(stopping(nextTimeout))) {
            LOGI("%s idle stoppong.", getName().c_str());
            break;
        }

        int nev = 0;
        do {
            static const uint32_t maxTimeOut = 3000;
            if (nextTimeout != ~0ull) {
                nextTimeout = nextTimeout > maxTimeOut ? maxTimeOut : nextTimeout;
            } else {
                nextTimeout = maxTimeOut;
            }

            nev = epoll_wait(mEpollFd, events, maxEvents, nextTimeout);
            if (nev < 0 && errno == EINTR) {
            } else {
                break;
            }
        } while (true);

        // LOGD("%s() %ld event amount %d\n", __func__, Fiber::GetFiberID(), nev);
        std::vector<std::function<void()>> cbs;
        ListExpireTimer(cbs);
        if (!cbs.empty()) {
            schedule<std::vector<std::function<void()>>::iterator>(cbs.begin(), cbs.end());
            cbs.clear();
        }
        
        for (int i = 0; i < nev; ++i) {
            epoll_event &event = events[i];

            if (event.data.fd == mTickleFd[0]) {
                uint8_t buf[128] = {0};
                while (read(mTickleFd[0], buf, sizeof(buf)) > 0);
                continue;
            }

            Context *ctx = static_cast<Context *>(event.data.ptr);
            AutoLock<Mutex> lock(ctx->mutex);
            if (event.events & (EPOLLERR | EPOLLHUP)) {
                event.events |= (EPOLLIN | EPOLLOUT) & ctx->events;
            }
            int realEvents = IOManager::NONE;
            if (event.events & EPOLLIN) {
                realEvents |= IOManager::READ;
            }
            if (event.events & EPOLLOUT) {
                realEvents |= IOManager::WRITE;
            }
            if ((ctx->events & realEvents) == IOManager::NONE) {
                continue;
            }

            int leftEvents = (ctx->events & ~realEvents);
            int opt = leftEvents ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
            event.events = EPOLLET | leftEvents;
            int ret = epoll_ctl(mEpollFd, opt, ctx->fd, &event);
            if (ret) {
                LOGE("epoll_ctl(%d, EPOLL_CTL_DEL, %d, nullptr) error. [%d,%s]", mEpollFd,
                    ctx->fd, errno, strerror(errno));
                continue;
            }

            if (realEvents & IOManager::READ) {
                ctx->triggerEvent(IOManager::READ);
            }
            if (realEvents & IOManager::WRITE) {
                ctx->triggerEvent(IOManager::WRITE);
            }
        }

        Fiber::SP ptr = Fiber::GetThis();
        auto rawPtr = ptr.get();
        ptr.reset();
        rawPtr->back();
    }
}

void IOManager::tickle()
{
    if (!hasIdleThread()) {
        return;
    }
    int ret = write(mTickleFd[1], "T", 1);
    LOG_ASSERT2(ret == 1);
}

bool IOManager::stopping()
{
    uint64_t timeout = 0;
    return stopping(timeout);
}

void IOManager::onTimerInsertedAtFront()
{
    tickle();
}

IOManager::Context::EventContext& IOManager::Context::getContext(IOManager::Event event)
{
    switch (event) {
    case IOManager::READ:
        return read;
    case IOManager::WRITE:
        return write;
    default:
        LOG_ASSERT(false, "%s() invalid param", __func__);
    }
    throw Exception("never reach here");
}

void IOManager::Context::resetContext(IOManager::Context::EventContext& ctx)
{
    ctx.scheduler = nullptr;
    ctx.fiber.reset();
    ctx.cb = nullptr;
}

void IOManager::Context::triggerEvent(IOManager::Event event)
{
    LOG_ASSERT2(event & events);
    events = (Event)(events & ~event);
    EventContext &eventCtx = getContext(event);
    if (eventCtx.cb) {
        eventCtx.scheduler->schedule(&eventCtx.cb);
    } else {
        eventCtx.scheduler->schedule(&eventCtx.fiber);
    }

    eventCtx.scheduler = nullptr;
    return;
}

void IOManager::contextResize(uint32_t size)
{
    if (size == mContextVec.size()) {
        return;
    }
    if (size < mContextVec.size()) {
        for (uint32_t i = size; i < mContextVec.size(); ++i) {
            if (mContextVec[i] != nullptr) {
                delete mContextVec[i];
                mContextVec[i] = nullptr;
            }
        }
    }
    mContextVec.resize(size);
    uint32_t i = 0;
    for (auto &it : mContextVec) {
        if (it == nullptr) {
            it = new Context;
            it->fd = i++;
        }
    }
}

bool IOManager::stopping(uint64_t& timeout)
{
    timeout = getNearTimeout();
    return timeout == ~0ull
        && Scheduler::stopping();
}

} // namespace eular
