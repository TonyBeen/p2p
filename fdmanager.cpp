/*************************************************************************
    > File Name: fdmanager.cpp
    > Author: hsz
    > Brief:
    > Created Time: Mon 21 Mar 2022 04:39:52 PM CST
 ************************************************************************/

#include "fdmanager.h"
#include "hook.h"

namespace eular {

FdContext::FdContext(int fd) :
    mIsInit(false),
    mIsSocket(false),
    mIsSysNonblock(false),
    mIsUserNonblock(false),
    mIsClosed(true),
    mFd(fd),
    recvTimeout(-1),
    sendTimeout(-1)
{
    init();
}

FdContext::~FdContext()
{

}

bool FdContext::init()
{
    if (mIsInit) {
        return true;
    }

    struct stat fdStat;
    if (-1 == fstat(mFd, &fdStat)) {
        mIsInit = false;
        mIsSocket = false;
    } else {
        mIsInit = true;
        mIsSocket = S_ISSOCK(fdStat.st_mode);
    }

    if (mIsSocket) {
        int flags = fcntl_f(mFd, F_GETFL, 0);
        if (!(flags & O_NONBLOCK)) {
            fcntl_f(mFd, F_SETFL, flags | O_NONBLOCK);
        }
        mIsSysNonblock = true;
    } else {
        mIsSysNonblock = false;
    }

    mIsUserNonblock = false;
    mIsClosed = false;

    return mIsInit;
}

void FdContext::setTimeOut(int type, uint64_t ms)
{
    struct timeval tv;
    if (type == SO_RCVTIMEO) {
        recvTimeout = ms;
    } else if (type = SO_SNDTIMEO) {
        sendTimeout = ms;
    } else {
        return;
    }

    tv.tv_sec = ms / 1000;
    tv.tv_usec = ms % 1000 * 1000;
    if (mIsSocket) {
        setsockopt_f(mFd, SOL_SOCKET, type, &tv, sizeof(timeval));
    }
}

uint64_t FdContext::getTimeOut(int type) const
{
    if (type == SO_RCVTIMEO) {
        return recvTimeout;
    } else if (type = SO_SNDTIMEO) {
        return sendTimeout;
    }
    return -1;
}

Fdmanager::Fdmanager()
{
    mFdCtxVec.resize(256);
}

Fdmanager::~Fdmanager()
{

}

FdContext::SP Fdmanager::get(int fd, bool needCreate)
{
    if (fd < 0) {
        return nullptr;
    }

    {
        RDAutoLock<RWMutex> rdlock(mRWMutex);
        if (fd >= mFdCtxVec.size()) {
            if (!needCreate) {
                return nullptr;
            }
        } else {
            if (mFdCtxVec[fd] || !needCreate) {
                return mFdCtxVec[fd];
            }
        }
    }

    WRAutoLock<RWMutex> wrlock(mRWMutex);
    FdContext::SP ctx(new FdContext(fd));
    if (fd >= mFdCtxVec.size()) {
        mFdCtxVec.resize(fd * 1.5);
    }
    mFdCtxVec[fd] = ctx;
    return ctx;
}

void Fdmanager::del(int fd)
{
    WRAutoLock<RWMutex> wrlock(mRWMutex);
    if (fd >= mFdCtxVec.size()) {
        return;
    }
    mFdCtxVec[fd].reset();
}

} // namespace eular
