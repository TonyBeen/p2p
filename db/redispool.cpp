/*************************************************************************
    > File Name: redispool.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2022-04-07 09:43:27 Thursday
 ************************************************************************/

#include "redispool.h"
#include "config.h"
#include <log/log.h>

#define LOG_TAG "redispool"

namespace eular {

RedisPool::RedisPool()
{
    mRedisInstanceCount = Config::Lookup<uint32_t>("redis.redis_amount", 4);
    LOG_ASSERT2(mRedisInstanceCount > 0);
    mRedisHost = Config::Lookup<String8>("redis.redis_host", "127.0.0.1");
    mRedisPort = Config::Lookup<uint32_t>("redis.redis_port", 6379);
    mPassWord = Config::Lookup<String8>("redis.redis_auth", "123456");
    LOGD("redis instance count = %u, redis host: %s, redis port: %u",
        mRedisInstanceCount, mRedisHost.c_str(), mRedisPort);

    mRedisInuse = (bool *)malloc(mRedisInstanceCount);
    LOG_ASSERT2(mRedisInuse != nullptr);
    memset(mRedisInuse, 0, mRedisInstanceCount);
    for (int i = 0; i < mRedisInstanceCount; ++i) {
        RedisInterface::SP ptr(new (std::nothrow)RedisInterface(mRedisHost, mRedisPort, mPassWord.c_str()));
        if (ptr->ping()) {
            LOGD("redis start");
        }
        LOG_ASSERT2(ptr != nullptr);
    }
}

RedisPool::~RedisPool()
{
    WRAutoLock<RWMutex> wrlock(mRWMutex);
    free(mRedisInuse);
    mRedisInuse = nullptr;
}

std::shared_ptr<RedisPool::RedisAPI> RedisPool::getRedis()
{
    int32_t index = -1;
    {
        RDAutoLock<RWMutex> rdlock(mRWMutex);
        for (uint32_t i = 0; i < mRedisInstanceCount; ++i) {
            if (mRedisInuse[i] != true) {
                index = i;
                break;
            }
        }
    }

    std::shared_ptr<RedisPool::RedisAPI> ptr;
    if (index >= 0 && index < mRedisInstanceCount) {
        {
            WRAutoLock<RWMutex> wrlock(mRWMutex);
            RedisPool::RedisAPI *api = new RedisPool::RedisAPI(index, mRedisHandle[index].get(), this);
            ptr.reset(api);
        }
        if (ptr->redisInterface()->ping() == false) {   // 测试连接失败
            if (ptr->redisInterface()->reconnect() == false) {  // 重连redis失败
                RDAutoLock<RWMutex> rdlock(mRWMutex);
                mRedisInuse[index] = false;
                return nullptr;
            }
        }
        return ptr;
    }

    return nullptr;
}

bool RedisPool::freeRedis(uint32_t index)
{
    LOG_ASSERT2(index >= 0 && index < mRedisInstanceCount);
    WRAutoLock<RWMutex> wrlock(mRWMutex);
    if (mRedisInuse[index]) {
        mRedisInuse[index] = false;
    } else {    // FIXME: Will this happen?
        return false;
    }

    return true;
}

RedisPool::RedisAPI::RedisAPI(uint32_t idx, RedisInterface *api, RedisPool *pool) :
    index(idx),
    interface(api),
    redisPool(pool)
{

}

RedisPool::RedisAPI::~RedisAPI()
{
    LOG_ASSERT2(redisPool->freeRedis(index));
}

RedisInterface *RedisPool::RedisAPI::redisInterface()
{
    return interface;
}

} // namespace eular
