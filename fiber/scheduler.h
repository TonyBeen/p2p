/*************************************************************************
    > File Name: scheduler.h
    > Author: hsz
    > Brief:
    > Created Time: Sun 27 Feb 2022 05:41:28 PM CST
 ************************************************************************/

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include "fiber.h"
#include "thread.h"
#include <utils/string8.h>
#include <utils/mutex.h>
#include <memory>
#include <vector>
#include <list>
#include <atomic>

namespace eular {
class Scheduler
{
public:
    typedef std::shared_ptr<Scheduler> SP;

    /**
     * @brief 协程调度器
     * 
     * @param threads 线程数量
     * @param useCaller 是否包含用户调用线程
     * @param name 调度器名字
     */
    Scheduler(uint32_t threads = 1, bool useCaller = true, const eular::String8 &name = "");
    virtual ~Scheduler();

    String8 getName() const { return mName; }
    static Scheduler* GetThis();
    static Fiber* GetMainFiber();

    void start();
    void stop();
    
    bool hasIdleThread() const { return mIdleThreadCount.load() > 0; }

    /**
     * @brief 调度函数
     * 
     * @param fc 协程或回调函数
     * @param th 线程ID
     */
    template<class FiberOrCb>
    void schedule(FiberOrCb fc, int th = -1)
    {
        bool needTickle = false;
        {
            AutoLock<Mutex> lock(mQueueMutex);
            needTickle = scheduleNoLock(fc, th);
        }
        if (needTickle) {
            tickle();
        }
    }

    template<class Iterator>
    void schedule(Iterator begin, Iterator end)
    {
        bool needTickle = false;
        {
            AutoLock<Mutex> lock(mQueueMutex);
            while (begin != end) {
                needTickle = scheduleNoLock(&*begin, -1) || needTickle;
                ++begin;
            }
        }
        if (needTickle) {
            tickle();
        }
    }

    void switchTo(int th = -1);

private:
    /**
     * @brief 协程信息结构体, 绑定哪个线程
     */
    struct FiberBindThread {
        Fiber::SP fiberPtr;         // 协程智能指针对象
        std::function<void()> cb;   // 协程执行函数
        int thread;                 // 内核线程ID

        FiberBindThread() : thread(-1) {}
        FiberBindThread(Fiber::SP sp, int th) : fiberPtr(sp), thread(th) {}
        FiberBindThread(Fiber::SP *sp, int th) : thread(th) { fiberPtr.swap(*sp); }
        FiberBindThread(std::function<void()> f, int th) : cb(f), thread(th) {}
        FiberBindThread(std::function<void()> *f, int th) : thread(th) { cb.swap(*f); }

        void reset()
        {
            fiberPtr = nullptr;
            cb = nullptr;
            thread = -1;
        }
    };

    template<class FiberOrCb>
    bool scheduleNoLock(FiberOrCb fc, int thread) {
        bool needTickle = mFiberQueue.empty();
        FiberBindThread ft(fc, thread);
        if(ft.fiberPtr || ft.cb) {
            mFiberQueue.push_back(ft);
        }
        return needTickle;
    }
    

protected:
    void run();
    void setThis();

    /**
     * @brief 线程空闲时执行idle协程
     */
    virtual void idle();

    /**
     * @brief 通知协程调度器有新任务
     */
    virtual void tickle();
    virtual bool stopping();


protected:
    std::vector<Thread::SP> mThreads;           // 线程数组
    std::vector<int>        mThreadIds;         // 线程id数组
    uint32_t                mThreadCount;       // 线程数量
    uint8_t                 mContainUserCaller; // 是否包含用户线程
    int                     mRootThread;        // userCaller为true时，为用户调用线程ID，false为-1
    bool                    mStopping;          // 是否停止

    std::atomic<uint32_t>   mActiveThreadCount = {0};
    std::atomic<uint32_t>   mIdleThreadCount = {0};

private:
    eular::String8          mName;              // 调度器名字
    eular::Mutex            mQueueMutex;        // 任务队列锁
    Fiber::SP               mRootFiber;         // userCaller为true时有效
    std::list<FiberBindThread> mFiberQueue;     // 待执行的协程队列
};

} // namespace eular

#endif // __SCHEDULER_H__
