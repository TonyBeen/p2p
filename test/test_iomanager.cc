/*************************************************************************
    > File Name: test_iomanager.cc
    > Author: hsz
    > Brief:
    > Created Time: 2022-04-08 13:45:08 Friday
 ************************************************************************/

#include "iomanager.h"
#include <log/log.h>

#define LOG_TAG "main"

void test()
{
    LOGD("%s() *** will sleep\n", __func__);
    sleep(1);
    LOGD("%s() *** fiber id %lu\n", __func__, eular::Fiber::GetFiberID());
}

int main(int argc, char **argv)
{
    LOGD("%s() start\n", __func__);
    eular::IOManager *iom = new eular::IOManager(2, true, "p2p");

    // auto id_1 = iom->addTimer(1000, [](){
    //     LOGI("%s() timer emit", __func__);
    // }, 1000);    // test ok

    // auto id_2 = iom->addTimer(2000, [id_1, iom](){
    //     bool flag = iom->delTimer(id_1);
    //     LOGD("delTimer flag %d", flag);
    // }); // test deltimer ok

    // int fd = open("./test.txt", O_RDWR | O_CREAT, 0664); 
    // LOG_ASSERT(fd > 0, "open error. [%d,%s]", errno, strerror(errno));   // 文件描述符无法放进epoll 会报错EPERM
    // iom->addEvent(fd, eular::IOManager::WRITE, [fd](){
    //     LOGI("lseek %ld", lseek(fd, SEEK_CUR, 0));
    //     write(fd, "test", 4);
    // });
    // iom->addEvent(fd, eular::IOManager::READ, [fd](){
    //     LOGI("lseek %ld", lseek(fd, SEEK_CUR, 0));
    //     char buf[128] = {0};
    //     read(fd, buf, sizeof(buf));
    //     LOGI("%s", buf);
    // });

    eular::IOManager *worker = new eular::IOManager(2, false, "worker IOManger");
    eular::IOManager *acceptWorker = new eular::IOManager(1, false, "accept IOManager");
    eular::IOManager *ioWorker = new eular::IOManager(4, false, "io IOManager");

    iom->schedule(std::bind(&test));
    iom->GetMainFiber()->resume();
    return 0;
}