/*************************************************************************
    > File Name: app.h
    > Author: hsz
    > Brief:
    > Created Time: Sun 27 Mar 2022 06:08:39 PM CST
 ************************************************************************/

#ifndef __EULAR_P2P_APP_H__
#define __EULAR_P2P_APP_H__

#include <utils/utils.h>
#include <utils/string8.h>

namespace eular {

class Application
{
public:
    Application();
    ~Application();

    bool init(int argc, char **argv);
    int  run();

protected:
    int runAsTerm();
    int runAsDaemon();
    void printHelp();
    int main();

private:
    int     mArgc;
    char**  mArgv;
    String8 mConfigPath;
    bool    mDaemonOrTerminal;  // true为daemon, false为terminal
    bool    mNeedHelp;
};

} // namespace eular

#endif // __EULAR_P2P_APP_H__
