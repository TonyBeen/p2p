/*************************************************************************
    > File Name: fibermutex.cpp
    > Author: hsz
    > Brief:
    > Created Time: Thu 24 Mar 2022 08:53:50 AM CST
 ************************************************************************/

#include "fibermutex.h"
#include <log/log.h>

#define LOG_TAG "FiberSemaphore"

namespace eular {

FiberSemaphore::FiberSemaphore(size_t concurrency)
{
    mConcurrency = concurrency;
}

FiberSemaphore::~FiberSemaphore()
{
    LOG_ASSERT2(mWaiters.empty());
}

bool FiberSemaphore::trywait()
{
    LOG_ASSERT2(Scheduler::GetThis());
    {
        AutoLock<Mutex> lock(mFiberMutex);
        if (mConcurrency > 0) {
            --mConcurrency;
            return true;
        }
        return false;
    }
}

void FiberSemaphore::wait()
{
    LOG_ASSERT2(Scheduler::GetThis());
    {
        AutoLock<Mutex> lock(mFiberMutex);
        if (mConcurrency > 0) {
            -mConcurrency;
            return;
        }
        mWaiters.push_back(std::make_pair(Scheduler::GetThis(), Fiber::GetThis()));
    }
    Fiber::Yeild2Hold();
}

void FiberSemaphore::notofy()
{
    LOG_ASSERT2(Scheduler::GetThis());
    {
        AutoLock<Mutex> lock(mFiberMutex);
        if (!mWaiters.empty()) {
            auto waiter = mWaiters.front();
            mWaiters.pop_front();
            waiter.first->schedule(waiter.second);
        } else {
            ++mConcurrency;
        }
    }
}

} // namespace eular
