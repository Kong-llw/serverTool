#pragma once

#include <memory>
#include <functional>
#include <string>
#include "TcpConnection.hpp"
#include "FrameProtocalCodec.hpp"
// 前向声明 Player，避免头文件循环包含
struct Player;

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    explicit ClientSession(std::shared_ptr<TcpConnection> conn);
    using MsgHandler = std::function<void(const FrameProtocolCodec::DecodedPacket&, const std::shared_ptr<ClientSession>)>;
    void setMsgHandler(MsgHandler handler);
    void setPlayerData(const std::shared_ptr<Player>& player){
        playerData_ = player;
    }
    std::shared_ptr<Player> GetPlayer() {
        return playerData_.lock(); // 尝试提升为 shared_ptr
    }
    bool IsLoggedIn() {
        return !playerData_.expired();
    }
    void sendMessage(const std::string& msg, ProtoInfo::ProtocolType ptype = ProtoInfo::ProtocolType::CHATMSG);
    void sendHeartBeat();

    void start();

    uint64_t getId() const;

    void closeConnection();

private:
    void onRawDataReceived(const char* data, size_t len);
    void onConnectionClose(uint64_t id);

    uint64_t sentMsgUid_ = 0;
    std::shared_ptr<TcpConnection> connection_;
    std::unique_ptr<FrameProtocolCodec> codec_;
    std::weak_ptr<Player> playerData_;
    MsgHandler msgHandler_;
};
