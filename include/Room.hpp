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

struct RoomInfo {
    uint64_t room_id;
    std::string room_code;
    std::string room_name;
    std::string password;
    uint64_t owner_id;
    size_t capacity;
    RoomState state;
    size_t player_count;
    std::vector<uint64_t> player_ids;
    std::unordered_map<uint64_t, bool> ready_status;
};

struct RoomInListInfo{
    std::string room_code;
    std::string room_name;
    size_t capacity;
    size_t player_count;
    RoomState state;
};

// 房间不直接依赖 Session 类型；通过 send callback 注入通信能力
// sendFunc(player_session_id, payload)
using SendCallback = std::function<void(uint64_t, const std::string&)>;

class Room {
public:
    Room(uint64_t id, uint64_t owner_id, size_t capacity, const std::string& room_name,
     const std::string room_code, const std::string password, SendCallback sender);
    ~Room();

    RoomInfo getInfo();
    RoomInListInfo getInListInfo();

    std::vector<uint64_t> getPlayerIds();
    // 加入/离开
    RoomResult join(uint64_t player_id);
    RoomResult leave(uint64_t player_id);

    // 玩家准备/取消准备
    RoomResult setReady(uint64_t player_id, bool ready);

    // 房主操作：设置容量、开始游戏
    RoomResult setCapacity(uint64_t operator_id, size_t newcap);
    RoomResult startGame(uint64_t operator_id);

    // 广播/发送
    void broadcast(const std::string& message);
    void sendTo(uint64_t player_id, const std::string& message);

    // 查询玩家是否在线/在房间
    bool containsPlayer(uint64_t player_id);

private:
    uint64_t id_;
    std::string room_name_;
    std::string room_code_;
    std::string password_;
    uint64_t owner_id_;
    size_t capacity_;
    RoomState state_;
    std::vector<uint64_t> players_; // ordered list
    std::unordered_map<uint64_t, bool> ready_map_;

    SendCallback sendFunc_;
    std::mutex mu_; // 保护房间状态

    void ensure_owner_after_leave_locked();
};
