/*************************************************************************
    > File Name: uuid.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2022-04-01 10:58:05 Friday
 ************************************************************************/

#include "uuid.h"

namespace eular {

UUID::UUID() :
    mIsInited(false),
    mUUIDString(64)
{
    memset(mUUID, 0, Md5::MD5_BUF_SIZE);
}

UUID::UUID(const String8 &data) :
    mIsInited(false),
    mUUIDString(64)
{
    memset(mUUID, 0, Md5::MD5_BUF_SIZE);
    init(data);
}

bool UUID::init(const String8 &data)
{
    if (mIsInited) {
        return true;
    }
    mData = data;
    if (md5.encode(mUUID, (const uint8_t *)mData.c_str(), mData.length()) < 0) {
        return false;
    }

    hex2String();

    mIsInited = true;
    return true;
}

bool UUID::reinit(const String8 &data)
{
    mData = data;
    mIsInited = false;
    if (md5.encode(mUUID, (const uint8_t *)mData.c_str(), mData.length()) < 0) {
        return false;
    }

    hex2String();

    mIsInited = true;
    return true;
}

const String8 &UUID::uuid() const
{
    return mUUIDString;
}

void UUID::hex2String()
{
    mUUIDString.clear();
    for (uint32_t i = 0; i < Md5::MD5_BUF_SIZE; ++i) {
        mUUIDString.appendFormat("%02x", mUUID[i]);
    }
}

String8 UUID::dump() const
{
    String8 ret;
    for (uint32_t i = 0; i < Md5::MD5_BUF_SIZE; ++i) {
        ret.appendFormat("0x%02x ", mUUID[i]);
    }
    return ret;
}

} // namespace eular
