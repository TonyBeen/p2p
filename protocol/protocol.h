/*************************************************************************
    > File Name: protocol.h
    > Author: hsz
    > Brief:
    > Created Time: Wed 09 Feb 2022 09:52:18 PM CST
 ************************************************************************/

#ifndef __EULAR_P2P_PROTOCOL_H__
#define __EULAR_P2P_PROTOCOL_H__

#include "endian.hpp"
#include <utils/Buffer.h>
#include <string>
#include <stdint.h>

#define __attribute_packed__        __attribute__((packed))

#define SPECIAL_IDENTIFIER 0x55647382
#define P2P_HEADER_SIZE 16

#define P2P_REQUEST                     0x0100
#define P2P_REQUEST_SEND_PEER_INFO      (P2P_REQUEST + 1)   // 发送本机信息
#define P2P_REQUEST_GET_PEER_INFO       (P2P_REQUEST + 2)   // 获取所有的主机信息
#define P2P_REQUEST_CONNECT_TO_PEER     (P2P_REQUEST + 3)   // 连接某一主机
#define P2P_REQUEST_HEARTBEAT_DETECT    (P2P_REQUEST + 4)   // 客户端响应心跳检测

#define P2P_RESPONSE                    0x1000
#define P2P_RESPONSE_SEND_PEER_INFO     (P2P_RESPONSE + 1)  // 服务端响应客户端发送的信息
#define P2P_RESPONSE_GET_PEER_INFO      (P2P_RESPONSE + 2)  // 服务端响应获取客户端信息
#define P2P_RESPONSE_CONNECT_TO_PEER    (P2P_RESPONSE + 3)  // 服务端响应客户端请求连接
#define P2P_RESPONSE_CONNECT_TO_ME      (P2P_RESPONSE + 4)  // 服务器响应对端有人要建立连接
#define P2P_RESPONSE_HEARTBEAT_DETECT   (P2P_RESPONSE + 5)  // 服务端用于udp的心跳检测包(由服务端主动发起)

#define UUID_SIZE       48
#define PEER_NAME_SIZE  32

/**
 * 
 * flag: 专用标志符 0x55 0x64 0x73 0x82
 * cmd: 命令
 * time: 发送此帧的时间
 * length: 携带的数据长度
 * data: 数据
 * 
 * |                 8byte                 |
 * |-------------------|-------------------|
 * |    flag(4byte)    | cmd(2b) |0x00 0x00|
 * |-------------------|-------------------|
 * |    time(4byte)    |   length(4byte)   |
 * |-------------------|-------------------|
 * |               data(...)               |
 * |-------------------|-------------------|
 * 
 */

class ProtocolParser
{
public:
    ProtocolParser();
    ~ProtocolParser();

    bool parse(const uint8_t *buf, size_t len);
    bool parse(const eular::ByteBuffer &buffer);

    uint16_t commnd() const;
    uint32_t time() const;
    uint32_t length() const;
    eular::ByteBuffer &data();

protected:
    uint16_t            mCommnd;
    uint32_t            mSendTime;
    eular::ByteBuffer   mDataBuffer;
};

class ProtocolGenerator
{
public:
    ProtocolGenerator() {}
    ~ProtocolGenerator() {}

    static eular::ByteBuffer generator(uint16_t cmd, const uint8_t *data, size_t len);
    static eular::ByteBuffer generator(uint16_t cmd, const eular::ByteBuffer &data);
};

// 服务端响应获取客户端结构体
typedef struct __Peer_Info {
    uint32_t    host_binary;                // 网络字节序二进制IP
    uint16_t    port_binary;                // 网络字节序端口号
    char        peer_uuid[UUID_SIZE];       // 对端在服务器中的ID
    char        peer_name[PEER_NAME_SIZE];  // 根据flag确定是对端名字还是本机名字
} __attribute_packed__ Peer_Info;
static const uint32_t Peer_Info_Size = sizeof(Peer_Info);

// 发送给服务端结构体
typedef struct __P2S_Request {
    uint16_t    flag;           // 请求种类
    Peer_Info   peer_info;
} __attribute_packed__ P2S_Request;
static const uint32_t P2S_Request_Size = sizeof(P2S_Request);

// 服务端响应结构体
typedef struct __P2S_Response {
    uint16_t    flag;
    uint16_t    statusCode; // 状态码
    char        msg[64];    // 原因描述
    uint32_t    number;     // 后面有多少个Peer_Info
} __attribute_packed__ P2S_Response;
static const uint32_t P2S_Response_Size = sizeof(P2S_Response);

// TODO: 修改为P2P需要的状态码
#define P2P_STATUS_MAP(XX)                                                    \
    XX(100, CONTINUE,                        Continue)                        \
    XX(101, SWITCHING_PROTOCOLS,             Switching Protocols)             \
    XX(102, PROCESSING,                      Processing)                      \
    XX(200, OK,                              OK)                              \
    XX(201, CREATED,                         Created)                         \
    XX(202, ACCEPTED,                        Accepted)                        \
    XX(203, NON_AUTHORITATIVE_INFORMATION,   Non-Authoritative Information)   \
    XX(204, NO_CONTENT,                      No Content)                      \
    XX(205, RESET_CONTENT,                   Reset Content)                   \
    XX(206, PARTIAL_CONTENT,                 Partial Content)                 \
    XX(207, MULTI_STATUS,                    Multi-Status)                    \
    XX(208, ALREADY_REPORTED,                Already Reported)                \
    XX(226, IM_USED,                         IM Used)                         \
    XX(300, REDIS_SERVER_ERROR,              Redis Server Error)              \
    XX(301, MOVED_PERMANENTLY,               Moved Permanently)               \
    XX(302, FOUND,                           Found)                           \
    XX(303, SEE_OTHER,                       See Other)                       \
    XX(304, NOT_MODIFIED,                    Not Modified)                    \
    XX(305, USE_PROXY,                       Use Proxy)                       \
    XX(307, TEMPORARY_REDIRECT,              Temporary Redirect)              \
    XX(308, PERMANENT_REDIRECT,              Permanent Redirect)              \
    XX(400, BAD_REQUEST,                     Bad Request)                     \
    XX(401, UNAUTHORIZED,                    Unauthorized)                    \
    XX(402, PAYMENT_REQUIRED,                Payment Required)                \
    XX(403, FORBIDDEN,                       Forbidden)                       \
    XX(404, NOT_FOUND,                       Not Found)                       \
    XX(405, METHOD_NOT_ALLOWED,              Method Not Allowed)              \
    XX(406, NOT_ACCEPTABLE,                  Not Acceptable)                  \
    XX(407, PROXY_AUTHENTICATION_REQUIRED,   Proxy Authentication Required)   \
    XX(408, REQUEST_TIMEOUT,                 Request Timeout)                 \
    XX(409, CONFLICT,                        Conflict)                        \
    XX(410, GONE,                            Gone)                            \
    XX(411, LENGTH_REQUIRED,                 Length Required)                 \
    XX(412, PRECONDITION_FAILED,             Precondition Failed)             \
    XX(413, PAYLOAD_TOO_LARGE,               Payload Too Large)               \
    XX(414, URI_TOO_LONG,                    URI Too Long)                    \
    XX(415, UNSUPPORTED_MEDIA_TYPE,          Unsupported Media Type)          \
    XX(416, RANGE_NOT_SATISFIABLE,           Range Not Satisfiable)           \
    XX(417, EXPECTATION_FAILED,              Expectation Failed)              \
    XX(421, MISDIRECTED_REQUEST,             Misdirected Request)             \
    XX(422, UNPROCESSABLE_ENTITY,            Unprocessable Entity)            \
    XX(423, LOCKED,                          Locked)                          \
    XX(424, FAILED_DEPENDENCY,               Failed Dependency)               \
    XX(426, UPGRADE_REQUIRED,                Upgrade Required)                \
    XX(428, PRECONDITION_REQUIRED,           Precondition Required)           \
    XX(429, TOO_MANY_REQUESTS,               Too Many Requests)               \
    XX(431, REQUEST_HEADER_FIELDS_TOO_LARGE, Request Header Fields Too Large) \
    XX(451, UNAVAILABLE_FOR_LEGAL_REASONS,   Unavailable For Legal Reasons)   \
    XX(500, INTERNAL_SERVER_ERROR,           Internal Server Error)           \
    XX(501, NOT_IMPLEMENTED,                 Not Implemented)                 \
    XX(502, BAD_GATEWAY,                     Bad Gateway)                     \
    XX(503, SERVICE_UNAVAILABLE,             Service Unavailable)             \
    XX(504, GATEWAY_TIMEOUT,                 Gateway Timeout)                 \
    XX(505, HTTP_VERSION_NOT_SUPPORTED,      HTTP Version Not Supported)      \
    XX(506, VARIANT_ALSO_NEGOTIATES,         Variant Also Negotiates)         \
    XX(507, INSUFFICIENT_STORAGE,            Insufficient Storage)            \
    XX(508, LOOP_DETECTED,                   Loop Detected)                   \
    XX(510, NOT_EXTENDED,                    Not Extended)                    \
    XX(511, NETWORK_AUTHENTICATION_REQUIRED, Network Authentication Required) \


enum class P2PStatus {
#define XX(code, name, desc) name = code,
    P2P_STATUS_MAP(XX)
#undef XX
    INVALID_STATUS
};

std::string Status2String(P2PStatus status);
P2PStatus String2Status(const std::string &str);

#endif // __EULAR_P2P_PROTOCOL_H__