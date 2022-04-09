/*************************************************************************
    > File Name: fiber.h
    > Author: hsz
    > Mail:
    > Created Time: Wed 22 Sep 2021 04:17:18 PM CST
 ************************************************************************/

#ifndef __FIBER_H__
#define __FIBER_H__

#include <stdio.h>
#include <ucontext.h>
#include <functional>
#include <memory>

namespace eular {
class Scheduler;
/**
 * @brief 协程类
 */
class Fiber : public std::enable_shared_from_this<Fiber>
{
public:
    typedef std::shared_ptr<Fiber> SP;
    enum FiberState {
        READY,      // 可执行态
        HOLD,       // 暂停状态
        EXEC,       // 执行状态
        TERM,       // 结束状态
        EXCEPT      // 异常状态
    };
    Fiber(std::function<void()> cb, uint64_t stackSize = 0);
    ~Fiber();

           void         reset(std::function<void()> cb);
    static void         SetThis(Fiber *f);  // 设置当前正在执行的协程
    static Fiber::SP    GetThis();          // 获取当前正在执行的协程
           void         call();             // 唤醒当前线程的协程
           void         back();             // 将当前线程的协程切到后台
           void         resume();           // 唤醒协程调度器的主协程
    static void         Yeild2Hold();       // 将当前正在执行的协程让出执行权给主协程，并设置状态为HOLD
    static void         Yeild2Ready();      // 将当前正在执行的协程让出执行权给主协程，并设置状态为READY
    FiberState          getState();         // 获取执行状态
    static uint64_t     GetFiberID();       // 获取当前协程ID

private:
    Fiber();                    // 线程的第一个协程调用
    static void FiberEntry();   // 协程入口函数
    void swapIn();              // 切换到前台, 获取执行权限
    void swapOut();             // 切换到后台, 让出执行权限

private:
    ucontext_t      mCtx;
    FiberState      mState;
    uint64_t        mFiberId;
    uint64_t        mStackSize;
    void *          mStack;
    std::function<void()> mCb;
    friend class Scheduler;
};

} // namespace eular

#endif // __FIBER_H__