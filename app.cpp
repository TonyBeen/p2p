/*************************************************************************
    > File Name: app.cpp
    > Author: hsz
    > Brief:
    > Created Time: Sun 27 Mar 2022 06:08:42 PM CST
 ************************************************************************/

#include "app.h"
#include "config.h"
#include "net/epoll.h"
#include "net/udpsocket.h"
#include "p2p_service.h"
#include "db/redispool.h"
#include <utils/string8.h>
#include <log/log.h>
#include <log/callstack.h>
#include <getopt.h>
#include <sys/wait.h>
#include <unistd.h>

#define LOG_TAG "application"

namespace eular {

static const char *defaultConfigPath = "./config/config.yaml";
static uint32_t gFatherPid = 0;
static uint32_t gSonPid = 0;

Application::Application() :
    mArgc(0),
    mArgv(nullptr),
    mConfigPath(defaultConfigPath),
    mDaemonOrTerminal(false),
    mNeedHelp(false)
{

}

Application::~Application()
{

}

bool Application::init(int argc, char **argv)
{
    bool ret = true;
    mArgc = argc;
    mArgv = argv;

    char c = 0;
    while ((c = getopt(argc, argv, "c:dh")) > 0) {
        switch (c) {
            case 'c':
                mConfigPath = optarg;
                break;
            case 'd':
                mDaemonOrTerminal = true;
                break;
            case 'h':
                mNeedHelp = true;
                break;
            case '?':
                printf("Invalid parameter\n");
                ret = false;
                break;
            default:
                break;
        }
    }

    if (mNeedHelp) {
        printHelp();
    }

    ConfigManager::get()->Init(mConfigPath);

    String8 level = Config::Lookup<String8>("log.level", "info");
    LogLevel::Level l = LogLevel::String2Level(level.c_str());
    bool isSync = Config::Lookup<bool>("log.sync", true);
    String8 target = Config::Lookup<String8>("log.target", "stdout");
    bool hasStdOut = target.contains("stdout");
    bool hasFileOut = target.contains("fileout");
    bool hasConsoleOut = target.contains("consoleout");
    InitLog(l, isSync);

    if (!hasStdOut) {
        delOutputNode(LogWrite::STDOUT);
    }
    if (hasFileOut) {
        addOutputNode(LogWrite::FILEOUT);
    }
    if (hasConsoleOut) {
        addOutputNode(LogWrite::CONSOLEOUT);
    }
    
    return ret;
}

int Application::run()
{
    if (mDaemonOrTerminal) {
        return runAsDaemon();
    } else {
        return runAsTerm();
    }
}

int Application::runAsTerm()
{
    return main();
}

int Application::runAsDaemon()
{
    daemon(1, 0);
    while (true) {
        pid_t pid = fork();
        if (pid == 0) { // 子进程
            LOGD("子进程开始: %d", getpid());
            return main();
        } else if (pid < 0) {   // 出错
            LOGE("%s() fork error. [%d, %s]", __func__, errno, strerror(errno));
            _exit(0);
        } else {    // 父进程
            gFatherPid = getpid();
            gSonPid = pid;
            int status = 0;
            waitpid(pid, &status, 0);
            if (status) {
                if (status == SIGKILL) {
                    LOGI("The child process(%d) was killed", pid);
                    break;
                }
            } else {
                break;
            }
        }
    }

    return 0;
}

void Signalcatch(int sig)
{
    LOGI("catch signal %d", sig);
    if (sig == SIGSEGV) {
        // 产生堆栈信息;
        CallStack stack;
        stack.update();
        stack.log("SIGSEGV", eular::LogLevel::FATAL);
    }

    if (sig == SIGABRT) {
        CallStack stack("SIGABRT");
    }

    if (sig == SIGUSR1) {
        if (gettid() == gFatherPid) {
            kill(gSonPid, SIGKILL);
        } else if (gettid() == gSonPid) {
            exit(SIGKILL);
        }
    }
    _exit(0);
}

int Application::main()
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGABRT, Signalcatch);
    signal(SIGSEGV, Signalcatch);
    signal(SIGUSR1, Signalcatch);

    RedisManager::get();
    uint32_t ioWorkerCount = Config::Lookup<uint32_t>("worker.io_worker_num", 4);
    uint32_t processWorkerCount = Config::Lookup<uint32_t>("worker.process_worker_num", 4);
    String8 tcphost = Config::Lookup<String8>("tcp.host", "0.0.0.0");
    uint16_t tcpport = Config::Lookup<uint16_t>("tcp.port", 12000);
    String8 udphost = Config::Lookup<String8>("udp.host", "0.0.0.0");
    uint16_t udpport = Config::Lookup<uint16_t>("udp.port", 12500);

    IOManager *acceptWorker = new (std::nothrow)IOManager(1, false, "accept-worker");
    IOManager *ioWorker = new (std::nothrow)IOManager(ioWorkerCount, false, "io-worker");
    IOManager *processWorker = new (std::nothrow)IOManager(processWorkerCount, true, "process-worker");
    LOG_ASSERT2(acceptWorker || ioWorker || processWorker);
    LOGI("acceptWorker %p; ioWorker %p; processWorker %p", acceptWorker, ioWorker, processWorker);
    acceptWorker->start();
    ioWorker->start();
    processWorker->start();

    Epoll::SP epoll(new (std::nothrow)Epoll(processWorker, ioWorker));
    UdpServer::SP udpServer(new (std::nothrow)UdpServer(epoll, ioWorker, processWorker));
    P2PService::SP p2pService(new (std::nothrow)P2PService(epoll, processWorker, ioWorker, acceptWorker));
    LOG_ASSERT2(epoll || udpServer || p2pService);
    LOGI("epoll: %p; udpserver: %p; p2pservice: %p", epoll.get(), udpServer.get(), p2pService.get());
    p2pService->bind(Address(tcphost, tcpport));
    udpServer->bind(Address(udphost, udpport));
    p2pService->listen();
    epoll->start();
    p2pService->start();
    udpServer->start();

    IOManager::GetMainFiber()->resume();
    LOGI("main exit");
    return 0;
}

void Application::printHelp()
{
    printf("usage: %s -[c|d|h...]\n", mArgv[0]);
    printf("********************************\n");
    printf("\t-c /path/to/config/\n");
    printf("\t-d start as daemon\n");
    printf("\t-h for print help\n");
    printf("********************************\n");
    _exit(0);
}

} // namespace eular
