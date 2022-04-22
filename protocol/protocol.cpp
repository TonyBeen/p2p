/*************************************************************************
    > File Name: protocol.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2022-04-08 11:27:19 Friday
 ************************************************************************/

#include "protocol.h"
#include <utils/utils.h>
#include <log/log.h>

#define LOG_TAG "protocol"

/**
 * 网路传输中不用关心字节的序列，因为都是按字节传输的。
 * 小端下传出的字节，其字节序就是小端的；大端下传出的字节，其字节序就是大端
 * 所以编码就显得尤为重要，以下编码统一生成小端字节序
 * 
 * 对于同一个数字 0x1234，在不同字节序的主机内的内存存放如下
 * little endian:   0x34 0x12
 * big endian:      0x12 0x34
 */

/**
 * @brief 编码n，并将n加到buf，buf指针向后偏移
 * 
 * @param buf 
 * @param n 
 * @return uint8_t* 返回偏移后的地址
 */
static inline uint8_t *encode8u(uint8_t *buf, uint8_t n)
{
    LOG_ASSERT2(buf != nullptr);
    *buf = n;
    return ++buf;
}

static inline const uint8_t *decode8u(const uint8_t *buf, uint8_t *n)
{
    LOG_ASSERT2(buf != nullptr);
    *n = *buf;

    return ++buf;
}

static inline uint8_t *encode16u(uint8_t *buf, uint16_t n)
{
#if EULAR_BYTE_ORDER == EULAR_BIG_ENDIAN
    *(buf + 0) = (n & 0xff);
    *(buf + 1) = (n >> 8);
#elif EULAR_BYTE_ORDER == EULAR_LITTLE_ENDIAN
    *(buf + 0) = n;
#else
#error "Unknown byte order. please check endian.hpp!"
#endif
    buf += 2;
    return buf;
}

static inline const uint8_t *decode16u(const uint8_t *buf, uint16_t *n)
{
#if EULAR_BYTE_ORDER == EULAR_BIG_ENDIAN
    *((uint8_t *)n + 0) = *(buf + 1);
    *((uint8_t *)n + 1) = *(buf + 0);
#elif EULAR_BYTE_ORDER == EULAR_LITTLE_ENDIAN
    *n = *(const uint16_t *)buf;
#else
#error "Unknown byte order. please check endian.hpp!"
#endif
    buf += 2;
    return buf;
}

static inline uint8_t *encode32u(uint8_t *buf, uint32_t n)
{
#if EULAR_BYTE_ORDER == EULAR_BIG_ENDIAN
    *(uint8_t *)(buf + 0) = (n >>  0) & 0xff;
    *(uint8_t *)(buf + 1) = (n >>  8) & 0xff;
    *(uint8_t *)(buf + 2) = (n >> 16) & 0xff;
    *(uint8_t *)(buf + 3) = (n >> 24) & 0xff;
#elif EULAR_BYTE_ORDER == EULAR_LITTLE_ENDIAN
    *(uint32_t *)buf = n;
#else
#error "Unknown byte order. please check endian.hpp!"
#endif
    buf += 4;
    return buf;
}

static inline const uint8_t *decode32u(const uint8_t *buf, uint32_t *n)
{
#if EULAR_BYTE_ORDER == EULAR_BIG_ENDIAN
    *((uint8_t *)n + 0) = *(buf + 3);
    *((uint8_t *)n + 1) = *(buf + 2);
    *((uint8_t *)n + 2) = *(buf + 1);
    *((uint8_t *)n + 3) = *(buf + 0);
#elif EULAR_BYTE_ORDER == EULAR_LITTLE_ENDIAN
    *n = *(const uint32_t *)buf;
#else
#error "Unknown byte order. please check endian.hpp!"
#endif
    buf += 4;
    return buf;
}

ProtocolParser::ProtocolParser() :
    mCommnd(0),
    mSendTime(0)
{

}

ProtocolParser::~ProtocolParser()
{

}

bool ProtocolParser::parser(const uint8_t *buf, size_t len)
{
    if (!buf || len < P2P_HEADER_SIZE) {
        return false;
    }
    uint32_t flag, length;
    uint16_t unused;

    buf = decode32u(buf, &flag);
    if (flag != SPECIAL_IDENTIFIER) {
        return false;
    }

    buf = decode16u(buf, &mCommnd);
    buf = decode16u(buf, &unused);
    buf = decode32u(buf, &mSendTime);
    buf = decode32u(buf, &length);

    mDataBuffer.clear();
    mDataBuffer.set(buf, length);
    return true;
}

bool ProtocolParser::parser(const eular::ByteBuffer &buffer)
{
    return parser(buffer.const_data(), buffer.size());
}

uint16_t ProtocolParser::commnd() const
{
    return mCommnd;
}

uint32_t ProtocolParser::time() const
{
    return mSendTime;
}

uint32_t ProtocolParser::length() const
{
    return mDataBuffer.size();
}

eular::ByteBuffer &ProtocolParser::data()
{
    return mDataBuffer;
}

eular::ByteBuffer ProtocolGenerator::generator(uint16_t cmd, const uint8_t *data, size_t len)
{
    eular::ByteBuffer buffer;
    uint8_t *temp;
    uint8_t *buf = (uint8_t *)malloc(P2P_HEADER_SIZE + len + 1);
    if (buf == nullptr) {
        goto ret;
    }
    temp = buf;
    temp = encode32u(temp, SPECIAL_IDENTIFIER);
    temp = encode16u(temp, cmd);
    temp = encode16u(temp, 0);
    temp = encode32u(temp, (uint32_t)Time::SystemTime());
    temp = encode32u(temp, len);
    memcpy(temp, data, len);

ret:
    if (buf) {
        free(buf);
    }
    return buffer;
}

eular::ByteBuffer ProtocolGenerator::generator(uint16_t cmd, const eular::ByteBuffer &data)
{
    return generator(cmd, data.const_data(), data.size());
}

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