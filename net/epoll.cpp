/*************************************************************************
    > File Name: epoll.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2022-03-28 10:33:40 Monday
 ************************************************************************/

#include "epoll.h"
#include "config.h"
#include <log/log.h>

#define LOG_TAG "epoll"

namespace eular {

Epoll::Epoll(IOManager *worker, IOManager *io_worker) :
    mShouldStop(true),
    mWorker(worker),
    mIOWorker(io_worker)
{
    mEventSize = Config::Lookup<uint32_t>("epoll.event_size", 5000);
    mEpollFd = epoll_create(mEventSize);
    LOG_ASSERT(mEpollFd > 0, "epoll_create error. [%d,%s]", errno, strerror(errno));
    mClientVec.resize(256);
    mContextVec.resize(256);
}

Epoll::~Epoll()
{
    stop();
    ::close(mEpollFd);
}

bool Epoll::start()
{
    if (!mShouldStop) {
        return true;
    }

    mShouldStop = false;
    mWorker->schedule(std::bind(&Epoll::event_loop, this));
    return true;
}

void Epoll::stop()
{
    if (mShouldStop) {
        return;
    }
    mShouldStop = true;
    mContextVec.clear();
    mClientVec.clear();
}

bool Epoll::addEvent(Socket::SP clientSock, Session::SP session, uint32_t event)
{
    AutoLock<Mutex> lock(mMutex);
    int fd = clientSock->socket();
    if (mClientVec.capacity() > fd && mClientVec[fd] != nullptr) {
        return true;
    }

    if (event & EPOLLIN || event & EPOLLOUT) {
        epoll_event ev;
        ev.events = EPOLLET | event;
        FDContext::SP ctx;
        if (mContextVec[fd] == nullptr) {
            ctx.reset(new (std::nothrow)FDContext(session));
            if (ctx == nullptr) {
                return false;
            }
        }

        ev.data.ptr = ctx.get();
        int ret = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, fd, &ev);
        if (ret) {
            LOGE("%s() epoll_ctl error. [%d, %s]", __func__, errno, strerror(errno));
            return false;
        }
        vectorResize(fd);
        mClientVec[fd].swap(clientSock);
        mContextVec[fd] = ctx;
        return true;
    }

    return false;
}

bool Epoll::addEvent(Socket::SP clientSock, std::function<void(int)> readCB,
                     std::function<void(int)> writeCB, uint32_t event)
{
    AutoLock<Mutex> lock(mMutex);
    int fd = clientSock->socket();
    if (mClientVec.capacity() > fd && mClientVec[fd] != nullptr) {
        return true;
    }

    if (event & EPOLLIN || event & EPOLLOUT) {
        epoll_event ev;
        ev.events = EPOLLET | event;
        FDContext::SP ctx;
        if (mContextVec[fd] == nullptr) {
            ctx.reset(new (std::nothrow)FDContext(fd, event, readCB, writeCB));
            if (ctx == nullptr) {
                return false;
            }
        }

        ev.data.ptr = ctx.get();
        int ret = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, fd, &ev);
        if (ret) {
            LOGE("%s() epoll_ctl error. [%d, %s]", __func__, errno, strerror(errno));
            return false;
        }
        vectorResize(fd);
        mClientVec[fd].swap(clientSock);
        mContextVec[fd] = ctx;
        return true;
    }

    return false;
}

bool Epoll::delEvent(Socket::SP clientSock, uint32_t event)
{
    AutoLock<Mutex> lock(mMutex);
    int fd = clientSock->socket();
    if (mClientVec.capacity() > fd) {
        if (event == 0) {
            auto &it = mClientVec[fd];
            if (it) {
                it.reset();
            }
            epoll_ctl(mEpollFd, EPOLL_CTL_DEL, fd, nullptr);
        } else {
            epoll_event ev;
            auto &it = mContextVec[fd];
            uint32_t realEvent = ~event & it->event;
            ev.events = realEvent;
            ev.data.ptr = mContextVec[fd].get();
            epoll_ctl(mEpollFd, EPOLL_CTL_MOD, fd, &ev);
        }
    }
    return true;
}

void Epoll::event_loop()
{
    epoll_event *events = new (std::nothrow)epoll_event[mEventSize];
    LOG_ASSERT2(events != nullptr);

    while (!mShouldStop) {
        int nev = epoll_wait(mEpollFd, events, mEventSize, 1000);
        if (nev < 0) {
            LOGE("%s() epoll_wait error. [%d,%s]", __func__, errno, strerror(errno));
            break;
        }

        for (int i = 0; i < nev; ++i) {
            auto &ev = events[i];
            FDContext *ctx = static_cast<FDContext *>(ev.data.ptr);
            LOG_ASSERT2(ctx != nullptr);

            if (ev.events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
                Socket::SP &it = mClientVec[ctx->fd];
                Address::SP addr = it->getRemoteAddr();
                LOGI("%s() client %s:%d quit.", __func__, addr->getIP().c_str(), addr->getPort());
                removeFromEpoll(ctx->fd);
                resetFromContextVec(ctx->fd);
                continue;
            }

            if (ev.events & EPOLLIN) {
                mIOWorker->schedule(std::bind(&FDContext::executeEvent, ctx, EPOLLIN));
            }

            if (ev.events & EPOLLOUT) {
                mIOWorker->schedule(std::bind(&FDContext::executeEvent, ctx, EPOLLOUT));
            }
        }
    }
}

// void Epoll::contextVectorResize(int fd)
// {
//     if (fd > mContextVec.capacity()) {
//         mContextVec.resize(fd * 1.5);
//     }
// }

// void Epoll::clientVectorResize(int fd)
// {
//     if (fd > mClientVec.capacity()) {
//         mClientVec.resize(fd * 1.5);
//     }
// }

void Epoll::vectorResize(int fd)
{
    if (fd > mContextVec.capacity()) {
        mContextVec.resize(fd * 1.5);
    }

    if (fd > mClientVec.capacity()) {
        mClientVec.resize(fd * 1.5);
    }
}

/**
 * @brief 将客户端socket从数组中删除，清除内存并将socket从epoll删除。数组的内容来自外界，所以需要清除
 * 
 * @param fd 
 */
void Epoll::removeFromEpoll(int fd)
{
    AutoLock<Mutex> lock(mMutex);
    int ret = epoll_ctl(mEpollFd, EPOLL_CTL_DEL, fd, nullptr);
    if (ret) {
        LOGE("%s() epoll_ctl error. [%d, %s]", __func__, errno, strerror(errno));
    }
    mClientVec[fd].reset();
    LOG_ASSERT2(mClientVec[fd] == nullptr);
}

/**
 * @brief 将context数组中的fd所在的位置重置，但不删除其堆内存
 * 
 * @param fd 位置
 */
void Epoll::resetFromContextVec(int fd)
{
    auto &it = mContextVec[fd];
    LOG_ASSERT2(it != nullptr);
    it->reset();
}

} // namespace eular
