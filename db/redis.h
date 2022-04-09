/*************************************************************************
    > File Name: redis.h
    > Author: hsz
    > Brief:
    > Created Time: Mon 10 Jan 2022 03:12:44 PM CST
 ************************************************************************/

#ifndef __EULAR_P2P_DB_REDIS_H__
#define __EULAR_P2P_DB_REDIS_H__

#include <utils/utils.h>
#include <utils/string8.h>
#include <utils/exception.h>
#include <utils/mutex.h>
#include <hiredis/hiredis.h>
#include <memory>
#include <vector>
#include <map>

namespace eular {

class RedisReply {
public:
    DISALLOW_COPY_AND_ASSIGN(RedisReply);
    typedef std::shared_ptr<RedisReply> SP;

    RedisReply(redisReply *reply);
    virtual ~RedisReply();

    int type() const { return mReply ? mReply->type : -1; }
    String8 string();
    int     int32();
    double  double64();
    std::vector<String8> array();

private:
    std::shared_ptr<redisReply> mReply;
    friend class RedisInterface;
};

class RedisInterface {
public:
    typedef std::shared_ptr<RedisInterface> SP;

    RedisInterface();
    RedisInterface(const String8 &ip, uint16_t port, const char *pwd);
    DISALLOW_COPY_AND_ASSIGN(RedisInterface);
    virtual ~RedisInterface();

    int  connect(const String8 &ip, uint16_t port = 6379, const char *pwd = nullptr);
    int  connecttimeout(const String8 &ip, uint16_t port, const char *pwd, uint32_t timeoutMs = 2000);
    bool reconnect();
    bool disconnect();
    bool authenticate(const char *pwd);

    RedisReply::SP command(const String8 &sql);

    bool ping();
    int selectDB(uint16_t dbNum);
    std::vector<String8> showKeys(const String8 &pattern); // not implemented
    int  setKeyValue(const String8 &key, const String8 &val);
    String8  getKeyValue(const String8 &key);
    int  getKeyValue(const std::vector<String8> &keyVec, std::vector<String8> &valVec);
    bool isKeyExist(const String8 &key);
    bool delKey(const String8 &key);
    bool setKeyLifeCycle(const String8 &key, uint64_t milliseconds, bool isTimeStamp = false);
    bool delKeyLifeCycle(const String8 &key);
    int64_t getKeyTTLMS(const String8 &key);
    int getAllKeys(std::vector<String8> &keyVec);

    // hash
    int hashCreateOrReplace(const String8 &key,
        const std::vector<std::pair<String8, String8>> &filedValue);
    int hashSetFiledValue(const String8 &key,
        const String8 &filed, const String8 &value);
    int hashGetKeyFiled(const String8 &key, const String8 &filed, String8 &ret);
    int hashGetKeyAll(const String8 &key, std::map<String8, String8> &ret);
    int hashDelFileds(const String8 &key, const char **filed, uint32_t fileds);
    int hashDelFileds(const String8 &key, const std::vector<String8> &filedVec);

    // list
    int listInsertFront(const String8 &key, const String8 &value);
    int listInsertFront(const String8 &key, const std::vector<String8> &valueVec);
    int listInsertBack(const String8 &key, const String8 &value);
    int listInsertBack(const String8 &key, const std::vector<String8> &valueVec);
    int listDeleteFront(const String8 &key, const String8 &value);
    int listDeleteBack(const String8 &key, const String8 &value);
    int listDeleteAll(const String8 &key, const String8 &value);
    int listPopFront(const String8 &key, String8 &value);
    int listGetAll(const String8 &key, std::vector<String8> &vec);
    ssize_t listLength(const String8 &key);

    const char *strerror(int no) const;
    void setHost(const String8 &ip) { mRedisHost = ip; }
    void setPort(const uint16_t &port) { mRedisPort = port; }
    void setPassword(const String8 &pwd) { mRedisPwd = pwd; }
    RedisReply::SP getRedisReply() { return std::make_shared<RedisReply>(mRedisReply); }

protected:
    redisContext   *mRedisCtx;
    redisReply     *mRedisReply;
    Mutex           mMutex;

    String8         mRedisHost;
    uint16_t        mRedisPort;
    String8         mRedisPwd;
};

}

#endif // __EULAR_P2P_DB_REDIS_H__
