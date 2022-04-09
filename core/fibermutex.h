/*************************************************************************
    > File Name: fibermutex.h
    > Author: hsz
    > Brief:
    > Created Time: Thu 24 Mar 2022 08:53:43 AM CST
 ************************************************************************/

#ifndef __EULAR_P2P_CORE_FIBER_MUTEX_H__
#define __EULAR_P2P_CORE_FIBER_MUTEX_H__

#include "fiber/scheduler.h"
#include <utils/mutex.h>
#include <list>
#include <stl_pair.h>

namespace eular {

class FiberSemaphore : public NonCopyAble
{
public:
    FiberSemaphore(size_t concurrency = 1);
    ~FiberSemaphore();

    bool trywait();
    void wait();
    void notofy();

private:
    Mutex mFiberMutex;
    std::list<std::pair<Scheduler *, Fiber::SP>> mWaiters;  // 协程等待队列
    size_t mConcurrency;
};

} // namespace eular

#endif // __EULAR_P2P_CORE_FIBER_MUTEX_H__
