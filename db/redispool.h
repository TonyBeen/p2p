/*************************************************************************
    > File Name: redispool.h
    > Author: hsz
    > Brief:
    > Created Time: 2022-04-07 09:43:23 Thursday
 ************************************************************************/

#ifndef __EULAR_DB_REDIS_POOL_H__
#define __EULAR_DB_REDIS_POOL_H__

#include "redis.h"
#include <utils/singleton.h>
#include <utils/utils.h>
#include <utils/mutex.h>
#include <memory>

namespace eular {

class RedisPool
{
    friend class Singleton<RedisPool>;
    DISALLOW_COPY_AND_ASSIGN(RedisPool);
public:
    ~RedisPool();

    struct RedisAPI {
    private:
        uint32_t index;
        RedisInterface *interface;
        RedisPool *redisPool;

    public:
        RedisAPI(uint32_t idx, RedisInterface *api, RedisPool *pool);
        ~RedisAPI();
        RedisInterface *redisInterface();
    };

    std::shared_ptr<RedisAPI> getRedis();

private:
    RedisPool();
    /**
     * @brief 释放index位置的redis, 无需主动调用
     * 
     * @param index 
     * @return 成功返回true，失败返回false
     */
    bool freeRedis(uint32_t index);

private:
    uint32_t    mRedisInstanceCount;    // redis实例总量
    bool*       mRedisInuse;            // 正在使用的redis
    String8     mRedisHost;             // redis监听的ip
    uint32_t    mRedisPort;             // redis监听的端口
    String8     mPassWord;              // redis密码
    RWMutex     mRWMutex;               // for mRedisInuse && mRedisHandle
    std::vector<RedisInterface::SP> mRedisHandle;   // RedisInterface数组
};

typedef Singleton<RedisPool> RedisManager;

} // namespace eular

#endif // __EULAR_DB_REDIS_POOL_H__
