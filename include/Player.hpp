#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include "ClientSession.hpp"

struct PlayerDataInRoom{
    uint32_t seat_index;
    uint8_t color;
    uint8_t team;
    bool ready;
    bool is_owner;
};

struct Player {
    std::weak_ptr<ClientSession> session;
    int PlayerId = -1;  // 数据库里的用户ID
    uint64_t CurrentRoomId = 0;
    std::string name;
    
    PlayerDataInRoom RoomData;

    void CallSend(const std::string& msg) {
        // 尝试锁定 session，如果 session 还活着，就发送
        if (auto s = session.lock()) {
            s->sendMessage(msg); 
        }
    }
};

