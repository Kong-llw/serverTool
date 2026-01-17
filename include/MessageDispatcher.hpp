#pragma once

#include <memory>
#include <functional>
#include <string>
#include <asio.hpp>
#include "ClientSession.hpp"
#include "RoomManager.hpp"
#include "PlayerManager.hpp"
#include "FrameProtocalCodec.hpp"

class MessageDispatcher {
public:
    using BroadcastFn = std::function<int(uint64_t, const std::string&)>;

    bool Login(std::shared_ptr<ClientSession> session, int userId);
    MessageDispatcher(asio::any_io_executor exec, RoomManager* rm, PlayerManager* pm);
    void ProcessMessagePkt(const FrameProtocolCodec::DecodedPacket& pkt, const std::shared_ptr<ClientSession>& senderSession);

private:
    void ParseRoomReq(const std::string& req, const std::shared_ptr<ClientSession>& senderSession);

    asio::any_io_executor executor_;
    RoomManager* roomManager_;
    PlayerManager* playerManager_;
};
