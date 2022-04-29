/*************************************************************************
    > File Name: scheduler.cpp
    > Author: hsz
    > Brief:
    > Created Time: Sun 27 Feb 2022 05:41:32 PM CST
 ************************************************************************/

#include "scheduler.h"
#include "hook.h"
#include <utils/utils.h>
#include <log/log.h>

#define LOG_TAG "scheduler"

namespace eular {

static thread_local Scheduler *gScheduler = nullptr;    // 线程调度器
static thread_local Fiber *gMainFiber = nullptr;        // 调度器的主协程

Scheduler::Scheduler(uint32_t threads, bool useCaller, const eular::String8 &name) :
    mContainUserCaller(useCaller),
    mStopping(true),
    mName(name)
{
    LOGD("%s() start thread num: %d, name: %s", __func__, threads, name.c_str());
    LOG_ASSERT(threads > 0, "%s %s:%s() Invalid Param", __FILE__, __LINE__, __func__);
    if (useCaller) {
        Fiber::GetThis();
        --threads;

        gScheduler = this;
        mRootFiber.reset(new Fiber(std::bind(&Scheduler::run, this)));
        Thread::SetName(name);

        LOGD("root thread id %ld", gettid());
        mThreadIds.push_back(gettid());
        gMainFiber = mRootFiber.get();
        mRootThread = gettid();
    } else {
        mRootThread = -1;
    }

    mThreadCount = threads;
}

Scheduler::~Scheduler()
{
    LOG_ASSERT(mStopping, "You should call stop before deconstruction");
    if (gScheduler == this) {
        gScheduler = nullptr;
    }
}

Scheduler *Scheduler::GetThis()
{
    return gScheduler;
}

Fiber *Scheduler::GetMainFiber()
{
    return gMainFiber;
}

void Scheduler::start()
{
    AutoLock<Mutex> lock(mQueueMutex);
    if (!mStopping) {
        return;
    }

    mStopping = false;
    LOG_ASSERT(mThreads.empty(), "should be empty before start()");
    mThreads.resize(mThreadCount);
    for (size_t i = 0; i < mThreadCount; ++i) {
        mThreads[i].reset(new Thread(std::bind(&Scheduler::run, this), mName + "_" + std::to_string(i)));
        mThreadIds.push_back(mThreads[i]->getTid());
        LOGD("thread [%s:%d] start", mThreads[i]->getName().c_str(), mThreads[i]->getTid());
    }
}

void Scheduler::stop()
{
    if (mRootFiber && mThreadCount == 0 && 
        mRootFiber->getState() == Fiber::TERM) {
        LOGI("%s()", __func__);

        if (stopping()) {
            return;
        }
    }

    if (mRootThread != -1) {
        LOG_ASSERT(GetThis() == this, "");
    } else {
        LOG_ASSERT(GetThis() != this, "");
    }

    mStopping = true;
    for (size_t i = 0; i < mThreadCount; ++i) {
        tickle();
    }

    if (mRootFiber) {
        tickle();
    }

    if (mRootFiber && !stopping()) {
        mRootFiber->call();
    }

    std::vector<Thread::SP> threads;
    {
        AutoLock<Mutex> lock(mQueueMutex);
        threads.swap(mThreads);
    }

    for (auto &it : threads) {
        it->join();
    }
}

// 将当前协程切换到th线程
void Scheduler::switchTo(int th)
{
    LOG_ASSERT(Scheduler::GetThis() != nullptr, "");
    if (Scheduler::GetThis() == this) {
        if (th == -1 || th == gettid()) {
            return;
        }
    }

    schedule(Fiber::GetThis(), th);
    Fiber::Yeild2Hold();
}

void Scheduler::run()
{
    LOGI("Scheduler::run() in %s:%d", Thread::GetName().c_str(), gettid());
    setHookEnable(true);
    setThis();
    if (gettid() != mRootThread) {
        gMainFiber = Fiber::GetThis().get();
    }
    Fiber::SP idleFiber(new Fiber(std::bind(&Scheduler::idle, this)));
    Fiber::SP cbFiber(nullptr);

    FiberBindThread ft;
    while (true) {
        ft.reset();
        bool needTickle = false;
        bool isActive = false;
        {
            AutoLock<Mutex> lock(mQueueMutex);
            auto it = mFiberQueue.begin();
            while (it != mFiberQueue.end()) {
                if (it->thread != -1 && it->thread != gettid()) {   // 不满足线程ID一致的条件
                    ++it;
                    needTickle = true;
                    continue;
                }

                LOG_ASSERT(it->fiberPtr || it->cb, "task can not be null");
                if (it->fiberPtr && it->fiberPtr->getState() == Fiber::EXEC) {  // 找到的协程处于执行状态
                    ++it;
                    continue;
                }

                // LOGD("Find executable tasks");
                ft = *it;
                mFiberQueue.erase(it++);
                ++mActiveThreadCount;
                isActive = true;
                break;
            }
            needTickle |= it != mFiberQueue.end();
        }

        if (needTickle) {
            tickle();
        }

        if (ft.fiberPtr && (ft.fiberPtr->getState() != Fiber::EXEC && ft.fiberPtr->getState() != Fiber::EXCEPT)) {
            ft.fiberPtr->resume();
            --mActiveThreadCount;
            if (ft.fiberPtr->getState() == Fiber::READY) {  // 用户主动设置协程状态为REDAY
                schedule(ft.fiberPtr);
            } else if (ft.fiberPtr->getState() != Fiber::TERM &&
                       ft.fiberPtr->getState() != Fiber::EXCEPT) {
                ft.fiberPtr->mState = Fiber::HOLD;
            }
            ft.reset();
        } else if (ft.cb) {
            if (cbFiber) {
                cbFiber->reset(ft.cb);
            } else {
                cbFiber.reset(new Fiber(ft.cb));
                LOG_ASSERT(cbFiber != nullptr, "");
            }
            ft.reset();
            cbFiber->resume();
            --mActiveThreadCount;
            if (cbFiber->getState() == Fiber::READY) {  // 用户在callback函数内部调用Yeild2Ready
                schedule(cbFiber);
                cbFiber.reset();
            } else if (cbFiber->getState() == Fiber::EXCEPT ||
                    cbFiber->getState() == Fiber::TERM) {
                cbFiber->reset(nullptr);
            } else {    // 用户主动让出协程，需要将当前协程状态设为暂停态HOLD
                cbFiber->mState = Fiber::HOLD;
                cbFiber.reset();
            }
        } else {
            if (isActive) {
                --mActiveThreadCount;
                continue;
            }

            if (idleFiber->getState() == Fiber::TERM) {
                LOGI("idle fiber term");
                break;
            }

            ++mIdleThreadCount;
            idleFiber->resume();
            --mIdleThreadCount;
            if (idleFiber->getState() != Fiber::TERM && 
                idleFiber->getState() != Fiber::EXCEPT) {
                idleFiber->mState = Fiber::HOLD;
            }
        }
    }
}

void Scheduler::setThis()
{
    gScheduler = this;
}

void Scheduler::idle()
{
    while (!stopping()) {
        LOGI("%s() fiber id: %lu", __func__, Fiber::GetFiberID());
        Fiber::Yeild2Hold();
    }
}

void Scheduler::tickle()
{
    LOGI("%s()", __func__);
}

bool Scheduler::stopping()
{
    AutoLock<Mutex> lock(mQueueMutex);
    return mStopping && mFiberQueue.empty() && mActiveThreadCount == 0;
}

} // namespace eular
