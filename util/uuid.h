/*************************************************************************
    > File Name: uuid.h
    > Author: hsz
    > Brief:
    > Created Time: 2022-04-01 10:58:02 Friday
 ************************************************************************/

#ifndef __EULAR_UTIL_UUID_H__
#define __EULAR_UTIL_UUID_H__

#include <crypto/md5.h>
#include <utils/string8.h>

namespace eular {

class UUID
{
public:
    UUID();
    UUID(const String8 &data);
    ~UUID() {}

    bool init(const String8 &data);
    bool reinit(const String8 &data);
    const String8 &uuid() const;
    String8 dump() const;

protected:
    void hex2String();

private:
    String8     mUUIDString;
    uint8_t     mUUID[Md5::MD5_BUF_SIZE];
    String8     mData;
    bool        mIsInited;
    Md5         md5;
};

} // namespace eular

#endif // __EULAR_UTIL_UUID_H__
