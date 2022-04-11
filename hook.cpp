/*************************************************************************
    > File Name: hook.cpp
    > Author: hsz
    > Brief:
    > Created Time: Mon 21 Mar 2022 04:01:59 PM CST
 ************************************************************************/

#include "hook.h"
#include <utils/Errors.h>
#include <log/log.h>
#include "iomanager.h"
#include "fdmanager.h"

#define LOG_TAG "hook"

namespace eular {

static thread_local bool gHookEnable = false;
static uint32_t gConnectTimeoutMs = 3000;

#define HOOK_FUN(XX) \
    XX(sleep)        \
    XX(usleep)       \
    XX(socket)       \
    XX(connect)      \
    XX(accept)       \
    XX(recv)         \
    XX(recvfrom)     \
    XX(recvmsg)      \
    XX(send)         \
    XX(sendto)       \
    XX(sendmsg)      \
    XX(close)        \
    XX(fcntl)        \
    XX(ioctl)        \
    XX(getsockopt)   \
    XX(setsockopt)

void hook_init() {
    static bool is_inited = false;
    if(is_inited) {
        return;
    }
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
    HOOK_FUN(XX);
#undef XX
    is_inited = true;
}

struct __Hook_Initer {
    __Hook_Initer()
    {
        hook_init();
    }
};

__Hook_Initer t_hook_initer;

bool isHookEnable()
{
    return gHookEnable;
}

void setHookEnable(bool falg)
{
    gHookEnable = falg;
}

} // namespace eular

struct timer_info {
    int cancelled = 0;
};

/**
 * @brief 执行系统调用
 * 
 * @tparam OriginFun 函数指针
 * @tparam Args 函数所需要的参数
 * @param fd 套接字描述符
 * @param fun hook住的原生函数指针
 * @param hook_fun_name 函数指针对应的函数名字
 * @param event 事件类型(READ || WRITE)
 * @param type SO_RCVTIMEO || SO_SNDTIMEO
 * @param args 参数
 * @return ssize_t 
 */
template<typename OriginFun, typename... Args>
static ssize_t do_io(int fd, OriginFun fun, const char* hook_fun_name,
        uint32_t event, int type, Args&&... args)
{
    LOGI("%s() fd: %d, %p, %s\n", __func__, fd, fun, hook_fun_name);
    if (!eular::gHookEnable) {
        return fun(fd, std::forward<Args>(args)...);
    }

    eular::FdContext::SP ctx = eular::FdManager::get()->get(fd);
    if (!ctx) {
        return fun(fd, std::forward<Args>(args)...);
    }

    if (ctx->isClosed()) {
        errno = EBADF;
        return eular::Status::UNKNOWN_ERROR;
    }

    if (!ctx->isSocket() || ctx->getUserNonoblock()) {
        return fun(fd, std::forward<Args>(args)...);
    }

    uint64_t to = ctx->getTimeOut(type);
    std::shared_ptr<timer_info> tinfo(new timer_info);

    ssize_t n = 0;
    eular::IOManager *iom = nullptr;
    std::weak_ptr<timer_info> winfo;
    uint64_t timer = 0;
    int ret;

retry:
    n = fun(fd, std::forward<Args>(args)...);
    while (n == -1 && errno == EINTR) {
        n = fun(fd, std::forward<Args>(args)...);
    }

    if (n == -1 && errno == EAGAIN) {
        iom = eular::IOManager::GetThis();
        winfo = tinfo;

        if (to > 0) {
            timer = iom->addConditionTimer(to, [winfo, fd, iom, event]() {
                auto sp = winfo.lock();
                if (!sp || sp->cancelled) {
                    return;
                }
                sp->cancelled = ETIMEDOUT;
                iom->cancelEvent(fd, (eular::IOManager::Event)(event));
            }, winfo);
        }

        ret = iom->addEvent(fd, (eular::IOManager::Event)(event));
        if (eular_unlikely(ret)) {
            LOGE("%s addEvent(%d, 0x%x) error.", hook_fun_name, fd, event);
            if (timer) {
                iom->delTimer(timer);
            }
            return eular::Status::UNKNOWN_ERROR;
        } else {
            eular::Fiber::Yeild2Hold();
            if (timer) {
                iom->delTimer(timer);
            }
            if (tinfo->cancelled) {
                errno = tinfo->cancelled;
                return eular::Status::UNKNOWN_ERROR;
            }
            goto retry;
        }
    }

    return eular::Status::OK;
}

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define XX(name) name ## _fun name ## _f = nullptr;
    HOOK_FUN(XX);
#undef XX

/**
 * @brief 休眠secondes. 不能在IOManager和Schedule内部调用，会破坏协程状态，导致异常
 * 
 * @param seconds 
 * @return unsigned int 
 */
unsigned int sleep(unsigned int seconds) {
    if (!eular::gHookEnable) {
        return sleep_f(seconds);
    }

    eular::Fiber::SP fiber = eular::Fiber::GetThis();
    eular::IOManager* iom = eular::IOManager::GetThis();
    LOG_ASSERT2(iom != nullptr);
    iom->addTimer(seconds * 1000, 
        std::bind((void(eular::Scheduler::*)(eular::Fiber::SP, int))&eular::IOManager::schedule, iom, fiber, -1));
    eular::Fiber::Yeild2Hold();
    return 0;
}

int usleep(useconds_t usec)
{
    if (!eular::gHookEnable) {
        return usleep_f(usec);
    }

    eular::Fiber::SP ptr = eular::Fiber::GetThis();
    eular::IOManager *iom = eular::IOManager::GetThis();
    iom->addTimer(usec / 1000, 
        std::bind((void(eular::Scheduler::*)(eular::Fiber::SP, int))&eular::IOManager::schedule, iom, ptr, -1));
    eular::Fiber::Yeild2Hold();
    return 0;
}

int socket(int domain, int type, int protocol) {
    if (!eular::gHookEnable) {
        return socket_f(domain, type, protocol);
    }
    int fd = socket_f(domain, type, protocol);
    if (fd == -1) {
        return fd;
    }
    eular::FdManager::get()->get(fd, true);
    return fd;
}

int connect_with_timeout(int fd, const struct sockaddr *addr, socklen_t addrlen, uint64_t ms)
{
    if (!eular::gHookEnable) {
        return connect_f(fd, addr, addrlen);
    }

    eular::FdContext::SP fdctx = eular::FdManager::get()->get(fd);
    if (!fdctx || fdctx->isClosed()) {
        errno = EBADF;
        return -1;
    }

    if (!fdctx->isSocket()) {
        return connect_f(fd, addr, addrlen);
    }

    if (fdctx->getUserNonoblock()) {
        return connect_f(fd, addr, addrlen);
    }

    int n = connect_f(fd, addr, addrlen);
    if (n == 0) {
        return 0;
    } else if (n != -1 || errno != EINPROGRESS) {
        return n;
    }

    // 连接出错了
    eular::IOManager *iom = eular::IOManager::GetThis();
    std::shared_ptr<timer_info> sinfo(new timer_info);
    std::weak_ptr<timer_info> winfo(sinfo);
    uint64_t timerUniqueId = 0;

    if (ms > 0) {
        timerUniqueId = iom->addConditionTimer(ms, [winfo, fd, iom]() {
            auto t = winfo.lock();
            if (!t || t->cancelled) {
                return;
            }
            t->cancelled = ETIMEDOUT;
            iom->cancelEvent(fd, eular::IOManager::WRITE);
        }, winfo);
    }

    int ret = iom->addEvent(fd, eular::IOManager::WRITE);
    if (ret == eular::Status::OK) {
        eular::Fiber::Yeild2Hold();
        if (timerUniqueId) {
            iom->delTimer(timerUniqueId);
        }
        if (sinfo->cancelled) {
            errno = sinfo->cancelled;
            return eular::Status::TIMED_OUT;
        }
    } else {
        if (timerUniqueId) {
            iom->delTimer(timerUniqueId);
        }
        LOGE("connect addEvent error.");
    }

    int error = 0;
    socklen_t len = sizeof(error);
    if (-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
        return eular::Status::UNKNOWN_ERROR;
    }
    if (!error) {
        return eular::Status::OK;
    }

    errno = error;
    return eular::Status::UNKNOWN_ERROR;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return connect_with_timeout(sockfd, addr, addrlen, eular::gConnectTimeoutMs);
}

int accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    int fd = do_io(s, accept_f, "accept", eular::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
    if(fd >= 0) {
        eular::FdManager::get()->get(fd, true);
    }
    return fd;
}

// ssize_t read(int fd, void *buf, size_t count)
// {
//     return do_io(fd, read_f, "read", eular::IOManager::READ, SO_RCVTIMEO, buf, count);
// }

// ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
// {
//     return do_io(fd, readv_f, "readv", eular::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
// }

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
    return do_io(sockfd, recv_f, "recv", eular::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    return do_io(sockfd, recvfrom_f, "recvfrom", eular::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    return do_io(sockfd, recvmsg_f, "recvmsg", eular::IOManager::READ, SO_RCVTIMEO, msg, flags);
}

// ssize_t write(int fd, const void *buf, size_t count) {
//     return do_io(fd, write_f, "write", eular::IOManager::WRITE, SO_SNDTIMEO, buf, count);
// }

// ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
//     return do_io(fd, writev_f, "writev", eular::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
// }

ssize_t send(int s, const void *msg, size_t len, int flags) {
    return do_io(s, send_f, "send", eular::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags);
}

ssize_t sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
    return do_io(s, sendto_f, "sendto", eular::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags, to, tolen);
}

ssize_t sendmsg(int s, const struct msghdr *msg, int flags) {
    return do_io(s, sendmsg_f, "sendmsg", eular::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd)
{
    if (!eular::gHookEnable) {
        return close_f(fd);
    }

    eular::FdContext::SP ctxsp = eular::FdManager::get()->get(fd);
    if (ctxsp) {
        auto iom = eular::IOManager::GetThis();
        if (iom) {
            iom->cancelAll(fd);
        }
        eular::FdManager::get()->del(fd);
    }
    return close_f(fd);
}

int fcntl(int fd, int cmd, ... /* arg */ ) {
    va_list va;
    va_start(va, cmd);
    switch(cmd) {
        case F_SETFL:
            {
                int arg = va_arg(va, int);
                va_end(va);
                eular::FdContext::SP ctx = eular::FdManager::get()->get(fd);
                if(!ctx || ctx->isClosed() || !ctx->isSocket()) {
                    return fcntl_f(fd, cmd, arg);
                }
                ctx->setUserNonblock(arg & O_NONBLOCK);
                if(ctx->getSysNonblock()) {
                    arg |= O_NONBLOCK;
                } else {
                    arg &= ~O_NONBLOCK;
                }
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETFL:
            {
                va_end(va);
                int arg = fcntl_f(fd, cmd);
                eular::FdContext::SP ctx = eular::FdManager::get()->get(fd);
                if(!ctx || ctx->isClosed() || !ctx->isSocket()) {
                    return arg;
                }
                if(ctx->getUserNonoblock()) {
                    return arg | O_NONBLOCK;
                } else {
                    return arg & ~O_NONBLOCK;
                }
            }
            break;
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
#ifdef F_SETPIPE_SZ
        case F_SETPIPE_SZ:
#endif
            {
                int arg = va_arg(va, int);
                va_end(va);
                return fcntl_f(fd, cmd, arg); 
            }
            break;
        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
#ifdef F_GETPIPE_SZ
        case F_GETPIPE_SZ:
#endif
            {
                va_end(va);
                return fcntl_f(fd, cmd);
            }
            break;
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
            {
                struct flock* arg = va_arg(va, struct flock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETOWN_EX:
        case F_SETOWN_EX:
            {
                struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        default:
            va_end(va);
            return fcntl_f(fd, cmd);
    }
}

int ioctl(int d, unsigned long int request, ...) {
    va_list va;
    va_start(va, request);
    void* arg = va_arg(va, void*);
    va_end(va);

    if (FIONBIO == request) {
        bool user_nonblock = !!*(int*)arg;
        eular::FdContext::SP ctx = eular::FdManager::get()->get(d);
        if(!ctx || ctx->isClosed() || !ctx->isSocket()) {
            return ioctl_f(d, request, arg);
        }
        ctx->setUserNonblock(user_nonblock);
    }
    return ioctl_f(d, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    if (!eular::gHookEnable) {
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
    if (level == SOL_SOCKET) {
        if(optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
            eular::FdContext::SP ctx = eular::FdManager::get()->get(sockfd);
            if(ctx) {
                const timeval* v = (const timeval*)optval;
                ctx->setTimeOut(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
            }
        }
    }
    return setsockopt_f(sockfd, level, optname, optval, optlen);
}

#ifdef __cplusplus
}
#endif // __cplusplus
