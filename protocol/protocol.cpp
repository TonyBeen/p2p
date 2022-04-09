/*************************************************************************
    > File Name: protocol.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2022-04-08 11:27:19 Friday
 ************************************************************************/

#include "protocol.h"

std::string Status2String(P2PStatus status)
{
#define XX(num, name, string)           \
    if (P2PStatus::name == status) {    \
        return #string;                 \
    }
    P2P_STATUS_MAP(XX);
#undef XX
    return "";
}

P2PStatus String2Status(const std::string &str)
{
#define XX(num, name, string)       \
    if (str == #string) {           \
        return P2PStatus::name;     \
    }
    P2P_STATUS_MAP(XX);
#undef XX
    return P2PStatus::INVALID_STATUS;
}