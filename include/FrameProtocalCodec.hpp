#ifndef FRAME_PROTOCAL_CODEC_HPP
#define FRAME_PROTOCAL_CODEC_HPP

// 包含所有依赖的头文件（客户端/服务端都需要）
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <arpa/inet.h>
#include <string_view>
#include <unordered_map>

// 协议头（确保客户端能访问）
#include "proto.h"

// 工具函数：用 inline 修饰，避免重定义
inline uint32_t adler32_update(uint32_t adler, const char* data, size_t len) {
    const uint32_t MOD = 65521;
    uint32_t a = adler & 0xFFFF;
    uint32_t b = (adler >> 16) & 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        a = (a + static_cast<unsigned char>(data[i])) % MOD;
        b = (b + a) % MOD;
    }
    return (b << 16) | a;
}

// 全局常量（constexpr 天然内联，无重定义问题）
constexpr size_t CHUNK_SIZE = 4 * 1024;          // 4KB
constexpr size_t METADATA_RESERVE = 64;
constexpr size_t MAX_DATA_SIZE = 10 * 1024 * 1024; // 10MB

class FrameProtocolCodec{
public:
    FrameProtocolCodec() = default;
    ~FrameProtocolCodec() = default;

    struct BinaryFrame {
        std::vector<char> data;
    };

    struct DecPktInfo {
        uint64_t msgId; 
        ProtoInfo::ProtocolType ptype;
    };
    struct DecodedPacket {
        DecPktInfo info;     
        std::string body;   // 实际数据
    };

    static std::vector<BinaryFrame> encode(uint64_t _msgId, const std::string& _msg, ProtoInfo::ProtocolType ptype = ProtoInfo::ProtocolType::CHATMSG){
        if (_msg.empty() || _msg.size() > MAX_DATA_SIZE) {
            throw std::invalid_argument("Input msg Err, size :" + std::to_string(_msg.size()));
        }

        std::vector<BinaryFrame> frames;
        uint32_t totalCheckSum = adler32_update(1, _msg.data(), _msg.size());
        //不需要拆分的包发送逻辑
        if(_msg.size() <= CHUNK_SIZE){
            BinaryFrame frame;
            ProtoHeader hdr = createProtoHeader(ProtoInfo::VERSION, ptype, _msg.size(), _msgId,
                ProtoFlags::SHORTMSG | ProtoFlags::CHECKSUM, 0, 0, totalCheckSum);
            frame.data.resize(sizeof(ProtoHeader) + _msg.size());
            std::memcpy(frame.data.data(), &hdr, sizeof(ProtoHeader));
            std::memcpy(frame.data.data() + sizeof(ProtoHeader), _msg.data(), _msg.size());
            frames.push_back(std::move(frame));
            return frames;
        }

        //拆分发送逻辑
        uint32_t totalParts = (_msg.size() + CHUNK_SIZE - 1) / CHUNK_SIZE;
        frames.reserve(totalParts);
        std::string_view strView(_msg);

        for (uint32_t i = 0; i < totalParts; i++) {
            size_t offset = static_cast<size_t>(i) * CHUNK_SIZE;
            std::string_view msgPiece = strView.substr(offset, CHUNK_SIZE);

            ProtoHeader hdr;
            if(i != totalParts - 1){
                uint32_t adl = adler32_update(1, reinterpret_cast<const char*>(msgPiece.data()), msgPiece.size());
                hdr = createProtoHeader(ProtoInfo::VERSION, ptype, msgPiece.size(), _msgId,
                    ProtoFlags::FRAGMENTED | ProtoFlags::CHECKSUM, i, totalParts, adl);
            }
            else{
                hdr = createProtoHeader(ProtoInfo::VERSION, ptype, msgPiece.size(), _msgId,
                    ProtoFlags::ENDPART | ProtoFlags::FRAGMENTED | ProtoFlags::CHECKSUM, i, totalParts, totalCheckSum);
            }
            
            BinaryFrame frame;
            frame.data.resize(sizeof(ProtoHeader) + msgPiece.size());
            std::memcpy(frame.data.data(), &hdr, sizeof(ProtoHeader));
            std::memcpy(frame.data.data() + sizeof(ProtoHeader), msgPiece.data(), msgPiece.size());

            frames.push_back(std::move(frame));
        }

        return frames;
    }

    void decode(const char* _data, size_t len, std::vector<DecodedPacket>& _outMsg){
        buffer_.insert(buffer_.end(), _data, _data+len);
        while(buffer_.size() >= PROTO_HEADER_SIZE){
            ProtoHeader* hdr = reinterpret_cast<ProtoHeader*>(buffer_.data());

            if (hdr->header != PROTO_MAGIC) {
                // 魔数不匹配 → 不是合法包的开头，逐字节后移1位，查找下一个魔数位置
                auto magicPtr = reinterpret_cast<const uint32_t*>(buffer_.data());
                bool foundMagic = false;
                // 遍历缓冲区，直到找到魔数 或 剩余数据不足4字节
                for (size_t i = 1; i <= buffer_.size() - sizeof(uint32_t); ++i) {
                    magicPtr = reinterpret_cast<const uint32_t*>(&buffer_[i]);
                    if (*magicPtr == PROTO_MAGIC) {
                        // 找到魔数 → 擦除前面的脏数据，锚定到正确位置
                        buffer_.erase(buffer_.begin(), buffer_.begin() + i);
                        foundMagic = true;
                        std::cerr << "Protocol Error: Magic mismatch, find magic at pos " << i << std::endl;
                        break;
                    }
                }
                // 遍历完没找到魔数 → 缓冲区全是脏数据，清空后退出
                if (!foundMagic) {
                    buffer_.clear();
                    std::cerr << "Protocol Error: No magic found, clear all dirty data" << std::endl;
                    return;
                }
                // 找到魔数后，重新进入循环，解析新的包头
                continue;
            }

            if(hdr->version != ProtoInfo::VERSION){
                buffer_.clear();
                std::cerr << "Protocol Error: Version Mismatch" << std::endl;
                return;
            }

            if(hdr->ptype == ProtoInfo::HEARTBEAT){
                buffer_.erase(buffer_.begin(), buffer_.begin() + sizeof(ProtoHeader));
                continue;
            }

            uint16_t payloadLen = ntohs(hdr->payload_len);
            size_t totalFrameLen = PROTO_HEADER_SIZE + payloadLen;

            if(buffer_.size() < totalFrameLen) break;

            try{
                handleSingleFrame(hdr, buffer_.data() + PROTO_HEADER_SIZE, payloadLen, _outMsg);
            } catch (const std::exception& e){
                std::cerr << "Frame Err:" << e.what() << std::endl;
                //断开连接
                ;
            }
            
            buffer_.erase(buffer_.begin(), buffer_.begin() + totalFrameLen);
        }
    }

    static BinaryFrame createHeartbeatFrame(){
        BinaryFrame rt;
        ProtoHeader hdr;
        std::memset(&hdr, 0, sizeof(ProtoHeader)); 
        hdr.header = PROTO_MAGIC; 
        hdr.version = ProtoInfo::VERSION;
        hdr.ptype = ProtoInfo::HEARTBEAT;
        rt.data.resize(sizeof(ProtoHeader));
        std::memcpy(rt.data.data(), &hdr, sizeof(ProtoHeader));

        return rt;
    }

    // 通用函数：支持 string, vector<char> 等
    std::string ToHex(const std::string& data) {
        std::stringstream ss;
        // 设置格式：16进制，大写，填充0
        ss << std::hex << std::uppercase << std::setfill('0');
        
        for (unsigned char c : data) {
            // 关键点：(int)c 强制把字符当数字处理
            // setw(2) 保证像 0x5 打印成 05
            ss << std::setw(2) << static_cast<int>(c) << " ";
        }
        return ss.str();
    }

    // 重载一个版本支持 vector<char> (如果你用 vector 存 buffer)
    std::string ToHex(const std::vector<char>& data) {
        std::string str(data.begin(), data.end());
        return ToHex(str);
    }

private:
    void handleSingleFrame(const ProtoHeader* _hdr, const char* _body, uint16_t _len, std::vector<DecodedPacket>& _outMsg){
        uint16_t flags = ntohs(_hdr->flags);
        uint64_t msg_id = (static_cast<uint64_t>(ntohl(_hdr->msg_id_high)) << 32) | ntohl(_hdr->msg_id_low);
        uint16_t seq = ntohs(_hdr->seq);
        uint16_t total = ntohs(_hdr->total);
        uint32_t expected_checksum = ntohl(_hdr->checksum);

        std::string a;
        a.append(_body,_len);
        std::cout << "handleSingleFrame Got Message" << ToHex(a) << std::endl;

        bool isFrag = (flags & ProtoFlags::FRAGMENTED);
        bool isEndpart = (flags & ProtoFlags::ENDPART);

        if(!isFrag){ //未经过分片
            uint32_t adl = adler32_update(1, _body, _len);
            if(adl != expected_checksum){
                throw std::runtime_error("Checksum mismatch! Data corrupted.");
            }
            DecodedPacket pkt{msg_id, static_cast<ProtoInfo::ProtocolType>(_hdr->ptype), ""};
            pkt.body.append(_body, _len);
            _outMsg.push_back(std::move(pkt));
            return;
        }
        
        //添加包内容
        auto& parts = fragCache_[msg_id];
        if(parts.empty()) parts.resize(total);
        if(seq < parts.size()) {
            parts[seq].assign(_body, _len);
        }   
        else{
            fragCache_.erase(msg_id);
            throw std::runtime_error("Invalid Seq");
        }
        
        //校验、拆分包合并处理
        if (!isEndpart){
            uint32_t adl = adler32_update(1, _body, _len);
            if(adl != expected_checksum){
                fragCache_.erase(msg_id);
                throw std::runtime_error("Checksum mismatch! Data corrupted.");
            }
        }
        else {
            std::string fullMsg;
            size_t totalLen = 0;
            for(const auto& s:parts)
                totalLen += s.size();
            fullMsg.reserve(totalLen);

            for(const auto& s:parts)
            {
                if(s.empty()) { // 缺包检查
                    fragCache_.erase(msg_id);
                    throw std::runtime_error("Msg Missing part");
                }
                fullMsg += s;
            }
            //对全部信息做一次checksum验证
            uint32_t adl = adler32_update(1, fullMsg.data(), fullMsg.size());
            if(adl != expected_checksum){
                throw std::runtime_error("Checksum mismatch! Data corrupted.");
            }
            DecodedPacket pkt{msg_id, static_cast<ProtoInfo::ProtocolType>(_hdr->ptype), std::move(fullMsg)};
            _outMsg.push_back(std::move(pkt));
            fragCache_.erase(msg_id);
        }
    }

    // createProtoHeader 方法：直接在类内实现
    static ProtoHeader createProtoHeader(uint8_t version, 
                                         ProtoInfo::ProtocolType ptype, 
                                         uint16_t payload_len, 
                                         uint64_t id, 
                                         uint16_t flags = 0, 
                                         uint16_t seq = 0, 
                                         uint16_t total = 0, 
                                         uint32_t checksum = 0) {
        ProtoHeader rt;
        rt.header = PROTO_MAGIC; //
        rt.version = version;
        rt.ptype = ptype;
        rt.payload_len = htons(payload_len);
        rt.flags = htons(flags);
        rt.msg_id_high =  htonl(static_cast<uint32_t>((id >> 32) & 0xFFFFFFFFu));
        rt.msg_id_low = htonl(static_cast<uint32_t>(id & 0xFFFFFFFFu));
        rt.seq = htons(seq);
        rt.total = htons(total);
        rt.checksum = htonl(checksum);

        return rt;
    }

    //成员变量
    std::vector<char> buffer_; //接收流 处理粘包
    std::unordered_map<uint64_t, std::vector<std::string>> fragCache_; //信息分片重组

};

#endif // FRAME_PROTOCAL_CODEC_HPP