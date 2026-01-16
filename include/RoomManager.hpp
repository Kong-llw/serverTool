// RoomManager.hpp
// 房间管理器（支持创建/加入/离开/列表/解散等），实现单例访问并支持注入 send 回调
#pragma once

#include "Room.hpp"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include <optional>

class RoomManager {
public:
    // 单例（线程安全）
    static RoomManager& instance();

    // 允许注入发送回调（建议在程序初始化时调用一次）
    void setSendCallback(SendCallback cb);

    // 创建房间，返回 room_id 或 0 on error，并设置 out_err
    uint64_t createRoom(uint64_t owner_id, size_t capacity, std::string& room_name, std::string& password, std::string& out_room_code, RoomResult& out_res);

    uint64_t getRoomId(const std::string& room_code);
    RoomResult joinRoom(uint64_t room_id, uint64_t player_id);
    RoomResult leaveRoom(uint64_t room_id, uint64_t player_id);

    // 房间列表（轻量返回信息）
    std::vector<RoomInListInfo> listRooms();

    std::optional<RoomInfo> getRoomInfo(uint64_t room_id);

    std::vector<uint64_t> getRoomPlayerIds(uint64_t room_id);

    RoomResult setReady(uint64_t room_id, uint64_t player_id, bool ready);
    RoomResult setCapacity(uint64_t room_id, uint64_t operator_id, size_t newcap);
    RoomResult startGame(uint64_t room_id, uint64_t operator_id);

    // 解散房间（调用者可基于权限/超时触发）
    RoomResult dissolveRoom(uint64_t room_id, uint64_t operator_id);

private:
    RoomManager();
    ~RoomManager() = default;
    RoomManager(const RoomManager&) = delete;
    RoomManager& operator=(const RoomManager&) = delete;

    std::unordered_map<uint64_t, std::shared_ptr<Room>> rooms_;
    std::unordered_map<std::string, uint64_t> code_to_id_;
    std::mutex mu_;
    std::atomic<uint64_t> next_room_id_ {1};
    SendCallback sendCb_;

    //std::weak_ptr<std::unordered_map<uint64_t, std::shared_ptr<ClientSession>>> activeSessionsWeakPtr_;
};

struct createRoomInfo{
    std::string room_name;
    std::string passwd;
};
