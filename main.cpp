/*************************************************************************
    > File Name: main.cpp
    > Author: hsz
    > Brief:
    > Created Time: Wed 23 Mar 2022 09:17:34 AM CST
 ************************************************************************/

#include "app.h"
#include <log/log.h>

int main(int argc, char **argv)
{
    eular::Application app;
    if (!app.init(argc, argv)) {
        return 0;
    }

    return app.run();
}
