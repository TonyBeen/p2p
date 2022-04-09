/*************************************************************************
    > File Name: timer.h
    > Author: hsz
    > Brief:
    > Created Time: Sun 20 Mar 2022 07:57:32 PM CST
 ************************************************************************/

#ifndef __EULAR_CORE_TIMER_H__
#define __EULAR_CORE_TIMER_H__

#include <utils/utils.h>
#include <utils/mutex.h>
#include <utils/singleton.h>
#include <sys/epoll.h>
#include <stdint.h>
#include <set>
#include <memory>
#include <functional>

namespace eular {
class TimerManager;
class Timer : public std::enable_shared_from_this<Timer>
{
public:
    typedef std::shared_ptr<Timer> SP;
    typedef std::function<void(void)> CallBack;
    ~Timer();

    Timer &operator=(const Timer& timer);

    uint64_t getTimeout() const { return mTime; }
    uint64_t getUniqueId() const { return mUniqueId; }
    void setNextTime(uint64_t timeMs) { mTime = timeMs; }
    void setCallback(CallBack cb) { mCb = cb; }
    void setRecycleTime(uint64_t ms) { mRecycleTime = ms; }

    void cancel();
    void refresh();
    void reset(uint64_t ms, CallBack cb, uint32_t recycle);

    static uint64_t CurrentTime();

private:
    Timer();
    Timer(uint64_t ms, CallBack cb, uint32_t recycle);
    Timer(const Timer& timer);

private:
    friend class TimerManager;
    struct Comparator {
        // 传给set的比较器，从小到大排序
        bool operator()(const Timer::SP &left, const Timer::SP &right) {
            if (left == nullptr && right == nullptr) {
                return false;
            }
            if (left == nullptr) {
                return true;
            }
            if (right == nullptr) {
                return false;
            }
            if (left->mTime == right->mTime) { // 时间相同，比较ID
                return left->mUniqueId < right->mUniqueId;
            }
            return left->mTime < right->mTime;
        }
    };

private:
    uint64_t    mTime;          // (绝对时间)下一次执行时间(ms)
    uint64_t    mRecycleTime;   // 循环时间ms
    CallBack    mCb;            // 回调函数
    uint64_t    mUniqueId;      // 定时器唯一ID
};

class TimerManager
{
    DISALLOW_COPY_AND_ASSIGN(TimerManager);
public:
    typedef std::set<Timer *, Timer::Comparator>::iterator TimerIterator;
    TimerManager();
    virtual ~TimerManager();

    uint64_t getNearTimeout();
    uint64_t addTimer(uint64_t ms, Timer::CallBack cb, uint32_t recycle = 0);
    uint64_t addConditionTimer(uint64_t ms, Timer::CallBack cb, std::weak_ptr<void> cond, uint32_t recycle = 0);
    bool delTimer(uint64_t uniqueId);

protected:
    void ListExpireTimer(std::vector<std::function<void()>> &cbs);
    uint64_t addTimer(Timer::SP timer);
    virtual void onTimerInsertedAtFront() = 0;

private:
    RWMutex mTimerRWMutex;
    bool mTickle = false;       // 是否触发onTimerInsertedAtFront
    std::set<Timer::SP, Timer::Comparator>  mTimers;        // 定时器集合
};

} // namespace eular

#endif // __EULAR_CORE_TIMER_H__