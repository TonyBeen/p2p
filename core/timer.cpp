/*************************************************************************
    > File Name: timer.cpp
    > Author: hsz
    > Brief:
    > Created Time: Sun 20 Mar 2022 07:57:38 PM CST
 ************************************************************************/

#include "timer.h"
#include <utils/utils.h>
#include <log/log.h>
#include <atomic>

#define LOG_TAG "timer"

namespace eular {

std::atomic<uint64_t>   gUniqueIdCount{0};

Timer::Timer() :
    mTime(0),
    mCb(nullptr),
    mRecycleTime(0)
{
    mUniqueId = ++gUniqueIdCount;
}

Timer::Timer(uint64_t ms, CallBack cb, uint32_t recycle) :
    mTime(CurrentTime() + ms),
    mCb(cb),
    mRecycleTime(recycle)
{
    mUniqueId = ++gUniqueIdCount;
}

Timer::Timer(const Timer& timer) :
    mTime(timer.mTime),
    mCb(timer.mCb),
    mRecycleTime(timer.mRecycleTime),
    mUniqueId(timer.mUniqueId)
{

}

Timer::~Timer()
{

}

Timer &Timer::operator=(const Timer& timer)
{
    mTime = timer.mTime;
    mCb = timer.mCb;
    mRecycleTime = timer.mRecycleTime;
    mUniqueId = timer.mUniqueId;

    return *this;
}

void Timer::cancel()
{
    mTime = 0;
    mCb = nullptr;
    mRecycleTime = 0;
}

/**
 * @brief 如果是循环定时器则刷新下次执行时间
 */
void Timer::refresh()
{
    if (mRecycleTime > 0) {
        mTime += mRecycleTime;
    }
}

void Timer::reset(uint64_t ms, CallBack cb, uint32_t recycle)
{
    if (ms < mTime || cb == nullptr) {
        return;
    }

    mTime = ms;
    mCb = cb;
    mRecycleTime = recycle;
}

/**
 * @brief 获取绝对时间
 * 
 * @return uint64_t 返回获得的绝对时间
 */
uint64_t Timer::CurrentTime()
{
    return Time::Abstime();
}

TimerManager::TimerManager()
{

}

TimerManager::~TimerManager()
{

}

/**
 * @brief 获取最近一个定时器的触发时间
 */
uint64_t TimerManager::getNearTimeout()
{
    RDAutoLock<RWMutex> rdlcok(mTimerRWMutex);
    mTickle = false;
    if (mTimers.empty()) {
        return ~0ull;
    }

    const Timer::SP &timer = *mTimers.begin();
    uint64_t nowMs = Timer::CurrentTime();
    if (nowMs >= timer->getTimeout()) {
        return 0;
    }
    return timer->getTimeout() - nowMs;
}

uint64_t TimerManager::addTimer(uint64_t ms, Timer::CallBack cb, uint32_t recycle)
{
    Timer::SP timer(new (std::nothrow)Timer(ms, cb, recycle));
    return addTimer(timer);
}

static void OnTimer(std::weak_ptr<void> cond, std::function<void()> cb)
{
    std::shared_ptr<void> temp = cond.lock();
    if (temp) {
        cb();
    }
}

uint64_t TimerManager::addConditionTimer(uint64_t ms, Timer::CallBack cb, std::weak_ptr<void> cond, uint32_t recycle)
{
    return addTimer(ms, std::bind(&OnTimer, cond, cb), recycle);
}

bool TimerManager::delTimer(uint64_t uniqueId)
{
    WRAutoLock<RWMutex> wrLock(mTimerRWMutex);
    bool flag = false;
    for (auto it = mTimers.begin(); it != mTimers.end();) {
        if ((*it)->mUniqueId == uniqueId) {
            mTimers.erase(it);
            flag = true;
            break;
        }
        ++it;
    }
    return flag;
}

void TimerManager::ListExpireTimer(std::vector<std::function<void()>> &cbs)
{
    uint64_t nowMS = Timer::CurrentTime();
    std::vector<Timer::SP> expired;
    {
        RDAutoLock<RWMutex> rdlock(mTimerRWMutex);
        if (mTimers.empty()) {
            return;
        }
    }
    WRAutoLock<RWMutex> wrlock(mTimerRWMutex);
    auto it = mTimers.begin();
    while (it != mTimers.end() && (*it)->mTime <= nowMS) {
        ++it;
    }
    expired.insert(expired.begin(), mTimers.begin(), it);
    mTimers.erase(mTimers.begin(), it);

    for (auto &timer : expired) {
        cbs.push_back(timer->mCb);
        if (timer->mRecycleTime) {
            timer->refresh();
            mTimers.insert(timer);
        }
    }
}

uint64_t TimerManager::addTimer(Timer::SP timer)
{
    if (!timer) {
        return 0;
    }
    LOGD("addTimer() %p", &mTimerRWMutex);
    mTimerRWMutex.wlock();
    auto it = mTimers.insert(timer).first;
    bool atFront = (it == mTimers.begin()) && !mTickle;
    if (atFront) {
        mTickle = true;
    }
    mTimerRWMutex.unlock();

    if (atFront) {
        onTimerInsertedAtFront();
    }

    return timer->getUniqueId();
}

} // namespace eular
