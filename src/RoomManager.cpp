// RoomManager.cpp
#include "RoomManager.hpp"
#include "RoomUtils.hpp"
#include <iostream>

RoomManager& RoomManager::instance() {
    static RoomManager mgr;
    return mgr;
}

RoomManager::RoomManager() {}

void RoomManager::setSendCallback(SendCallback cb) {
    std::lock_guard<std::mutex> lk(mu_);
    sendCb_ = std::move(cb);
}

uint64_t RoomManager::createRoom(uint64_t owner_id, size_t capacity, std::string& room_name, std::string& password, std::string& out_room_code, RoomResult& out_res) {
    if(capacity < 2 || capacity > 16) { out_res = RoomResult::INVALID_CAPACITY; return 0; }
    uint64_t id = next_room_id_.fetch_add(1);

    std::string code;
    for(int i = 0; i < 5; ++i) { // 尝试生成不冲突房间码
        code = RoomUtils::genRoomCode();
        std::lock_guard<std::mutex> lk(mu_);
        if(code_to_id_.find(code) == code_to_id_.end()) {
            code_to_id_[code] = id;
            break;
        }
        if(i == 4) {
            out_res = RoomResult::GEN_ROOMCODE_FAILED;
            return 0;
        }
    }
    std::shared_ptr<Room> r = std::make_shared<Room>(id, owner_id, capacity, room_name, code, password, sendCb_);
    {
        std::lock_guard<std::mutex> lk(mu_);
        rooms_.emplace(id, r);
    }
    out_room_code = code;
    out_res = RoomResult::OK;
    return id;
}

uint64_t RoomManager::getRoomId(const std::string& room_code){
    std::lock_guard<std::mutex> lk(mu_);
    auto it = code_to_id_.find(room_code);
    if(it == code_to_id_.end()) return 0;
    return it->second;
}

RoomResult RoomManager::joinRoom(uint64_t room_id, uint64_t player_id) {
    std::shared_ptr<Room> r;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = rooms_.find(room_id);
        if(it == rooms_.end()) return RoomResult::NOT_FOUND;
        r = it->second;
    }
    return r->join(player_id);
}

RoomResult RoomManager::leaveRoom(uint64_t room_id, uint64_t player_id) {
    std::shared_ptr<Room> r;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = rooms_.find(room_id);
        if(it == rooms_.end()) return RoomResult::NOT_FOUND;
        r = it->second;
    }
    RoomResult res = r->leave(player_id);
    // if empty, remove room
    RoomInfo info = r->getInfo();
    if(info.player_count == 0) {
        std::lock_guard<std::mutex> lk(mu_);
        rooms_.erase(room_id);
    }
    return res;
}

std::vector<RoomInListInfo> RoomManager::listRooms() {
    std::vector<RoomInListInfo> out;
    std::lock_guard<std::mutex> lk(mu_);
    out.reserve(rooms_.size());
    for(auto &p : rooms_) out.push_back(p.second->getInListInfo());
    return out;
}

std::optional<RoomInfo> RoomManager::getRoomInfo(uint64_t room_id) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = rooms_.find(room_id);
    if(it == rooms_.end()) return std::nullopt;
    return it->second->getInfo();
}

std::vector<uint64_t> RoomManager::getRoomPlayerIds(uint64_t room_id) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = rooms_.find(room_id);
    if(it == rooms_.end()) return {};
    RoomInfo info = it->second->getInfo();
    return info.player_ids;
}

RoomResult RoomManager::setReady(uint64_t room_id, uint64_t player_id, bool ready) {
    std::shared_ptr<Room> r;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = rooms_.find(room_id);
        if(it == rooms_.end()) return RoomResult::NOT_FOUND;
        r = it->second;
    }
    return r->setReady(player_id, ready);
}

RoomResult RoomManager::setCapacity(uint64_t room_id, uint64_t operator_id, size_t newcap) {
    std::shared_ptr<Room> r;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = rooms_.find(room_id);
        if(it == rooms_.end()) return RoomResult::NOT_FOUND;
        r = it->second;
    }
    return r->setCapacity(operator_id, newcap);
}

RoomResult RoomManager::startGame(uint64_t room_id, uint64_t operator_id) {
    std::shared_ptr<Room> r;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = rooms_.find(room_id);
        if(it == rooms_.end()) return RoomResult::NOT_FOUND;
        r = it->second;
    }
    return r->startGame(operator_id);
}

RoomResult RoomManager::dissolveRoom(uint64_t room_id, uint64_t operator_id) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = rooms_.find(room_id);
    if(it == rooms_.end()) return RoomResult::NOT_FOUND;
    auto r = it->second;
    RoomInfo info = r->getInfo();
    if(operator_id != info.owner_id) return RoomResult::NOT_OWNER;
    rooms_.erase(it);
    return RoomResult::OK;
}
