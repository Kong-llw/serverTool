// Room.hpp
// 房间数据类与接口（POCO 风格 + 简单操作接口）
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <memory>

#include "Player.hpp"
enum class RoomState { LOBBY = 0, RUNNING = 1 };

// 房间操作返回码
enum class RoomResult {
    OK = 0,
    FULL,
    NOT_FOUND,
    ALREADY_IN_ROOM,
    NOT_IN_ROOM,
    NOT_OWNER,
    INVALID_CAPACITY,
    ALREADY_RUNNING,
    NOT_READY,
    GEN_ROOMCODE_FAILED,

    EMPTY_REQ,
    NOT_AUTHORIZED,
    UNKNOWN_ERROR
};

struct RoomInListInfo{
    std::string room_code;
    std::string room_name;
    size_t capacity;
    size_t player_count;
    RoomState state;
};

struct RoomInfo {
    uint64_t room_id;
    std::string room_code;
    std::string room_name;
    std::string password;
    uint64_t owner_id;
    size_t capacity;
    RoomState state;
    size_t player_count;
    std::vector<PlayerDataInRoom> player_infos;
};

// 房间不直接依赖 Session 类型；通过 send callback 注入通信能力
// sendFunc(player_session_id, payload)
using SendCallback = std::function<void(uint64_t, const std::string&)>;

class Room {
public:
    Room(uint64_t id, const std::shared_ptr<Player>& owner, uint32_t capacity, const std::string& room_name,
     const std::string& room_code, const std::string& password, SendCallback sender);
    ~Room();

    RoomInfo getAllInfo();
    RoomInListInfo getInListInfo();
    std::vector<PlayerDataInRoom> getPlayerInfos();

    const std::vector<std::shared_ptr<Player>>& getPlayers();
    // 加入/离开
    RoomResult join(const std::shared_ptr<Player>&);
    RoomResult leave(const std::shared_ptr<Player>&);

    // 玩家准备/取消准备
    RoomResult setReady(const std::shared_ptr<Player>&, bool ready);
    // 房主操作：设置容量、开始游戏
    bool isRoomOwner(const std::shared_ptr<Player>&);
    RoomResult setCapacity(const std::shared_ptr<Player>&, uint32_t newcap);
    RoomResult startGame(const std::shared_ptr<Player>&);

    // 广播/发送
    void broadcast(const std::string& message);
    void sendTo(uint64_t playerId, const std::string& message);

    // 查询玩家是否在线/在房间
    bool containsPlayer(const std::shared_ptr<Player>&);
    std::shared_ptr<Player> GetPlayer(uint64_t playerId);
    bool roomIsEmpty();

private:
    uint64_t id_;
    std::string room_name_;
    std::string room_code_;
    std::string password_;
    std::shared_ptr<Player> owner;
    size_t capacity_;
    RoomState state_;
    std::vector<std::shared_ptr<Player>> players_;

    SendCallback sendFunc_;
    std::mutex mu_; // 保护房间状态

    void ensure_owner_after_leave_locked();
};
