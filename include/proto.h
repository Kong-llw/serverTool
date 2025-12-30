#ifndef PROJECT_PROTO_H
#define PROJECT_PROTO_H

#include <cstdint>
#include <arpa/inet.h>

constexpr size_t PROTO_HEADER_SIZE = 4 + 1 + 1 + 2 + 2 + 4 + 4 + 2 + 2 + 4;

namespace ProtoFlags{
    constexpr uint16_t SHORTMSG = 0x0;
    constexpr uint16_t FRAGMENTED = 0x1;
    constexpr uint16_t CHECKSUM = 0x2;
    constexpr uint16_t ENDPART = 0x4;   //最后一个特殊包，保存整条信息的
}

namespace ProtoInfo {
    constexpr uint8_t VERSION = 1;
    enum ProtocolType : uint8_t {
        NORMAL = 0
    };
}

#pragma pack(push, 1)
struct ProtoHeader {    //内容会以网络字节序写入
    uint32_t header; // = 0x48123123
    uint8_t version;
    uint8_t ptype;
    uint16_t payload_len;
    uint16_t flags; //ProtoFlags
    uint32_t msg_id_high;
    uint32_t msg_id_low;
    uint16_t seq;
    uint16_t total;
    uint32_t checksum;
};
#pragma pack(pop)

static_assert(sizeof(ProtoHeader) == PROTO_HEADER_SIZE, "ProtoHeader size mismatch");

//一个日志宏
#include <chrono>
#include <string>
#include <iostream>
#include <iomanip>
#define LOG_IMPL(level, msg) { \
    auto now = std::chrono::system_clock::now(); \
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000; \
    auto t = std::chrono::system_clock::to_time_t(now); \
    std::tm tm = *std::localtime(&t); \
    std::cout << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count() << "]"; \
    std::cout << " [" << level << "] "; \
    std::cout << "[" << __FILE__ << ":" << __LINE__ << "] "; \
    std::cout << msg << std::endl; \
}
#define LOG_INFO(msg) LOG_IMPL("INFO", msg)
#define LOG_WARN(msg) LOG_IMPL("WARN", msg)
#define LOG_ERROR(msg) LOG_IMPL("ERROR", msg)
#define LOG_DEBUG(msg) LOG_IMPL("DEBUG", msg)


#endif