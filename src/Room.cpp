// Room.cpp
#include "Room.hpp"
#include <algorithm>
#include <iostream>

Room::Room(uint64_t id, uint64_t owner_id, size_t capacity, const std::string& room_name,
     const std::string room_code, const std::string password, SendCallback sender)
    : id_(id), room_name_(room_name), room_code_(room_code), password_(password), owner_id_(owner_id), capacity_(capacity), state_(RoomState::LOBBY), sendFunc_(std::move(sender)) {
    players_.push_back(owner_id);
    ready_map_[owner_id] = false;
}

Room::~Room() {
}

RoomInfo Room::getInfo() {
    std::lock_guard<std::mutex> lk(mu_);
    return RoomInfo{
        .room_id = id_,
        .room_code = room_code_,
        .room_name = room_name_,
        .password = password_,
        .owner_id = owner_id_,
        .capacity = capacity_,
        .state = state_,
        .player_count = players_.size(),
        .player_ids = players_,
        .ready_status = ready_map_
    };
}

RoomInListInfo Room::getInListInfo() {
    std::lock_guard<std::mutex> lk(mu_);
    return RoomInListInfo{
        .room_code = room_code_, 
        .room_name = room_name_,
        .capacity = capacity_,
        .player_count = players_.size(),
        .state = state_ };
}

std::vector<uint64_t> Room::getPlayerIds() {
    std::lock_guard<std::mutex> lk(mu_);
    return players_;
}

RoomResult Room::join(uint64_t player_id) {
    std::lock_guard<std::mutex> lk(mu_);
    if(state_ != RoomState::LOBBY) return RoomResult::ALREADY_RUNNING;
    if(players_.size() >= capacity_) return RoomResult::FULL;
    if(std::find(players_.begin(), players_.end(), player_id) != players_.end()) return RoomResult::ALREADY_IN_ROOM;
    players_.push_back(player_id);
    ready_map_[player_id] = false;
    // notify join
    if(sendFunc_) sendFunc_(player_id, "JOINED_ROOM");
    return RoomResult::OK;
}

RoomResult Room::leave(uint64_t player_id) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = std::find(players_.begin(), players_.end(), player_id);
    if(it == players_.end()) return RoomResult::NOT_IN_ROOM;
    players_.erase(it);
    ready_map_.erase(player_id);
    // adjust owner if needed
    ensure_owner_after_leave_locked();
    // notify
    if(sendFunc_) sendFunc_(player_id, "LEFT_ROOM");
    return RoomResult::OK;
}

RoomResult Room::setReady(uint64_t player_id, bool ready) {
    std::lock_guard<std::mutex> lk(mu_);
    if(std::find(players_.begin(), players_.end(), player_id) == players_.end()) return RoomResult::NOT_IN_ROOM;
    ready_map_[player_id] = ready;
    return RoomResult::OK;
}

RoomResult Room::setCapacity(uint64_t operator_id, size_t newcap) {
    std::lock_guard<std::mutex> lk(mu_);
    if(operator_id != owner_id_) return RoomResult::NOT_OWNER;
    if(newcap < 2 || newcap > 16) return RoomResult::INVALID_CAPACITY;
    if(newcap < players_.size()) return RoomResult::INVALID_CAPACITY;
    capacity_ = newcap;
    return RoomResult::OK;
}

RoomResult Room::startGame(uint64_t operator_id) {
    std::lock_guard<std::mutex> lk(mu_);
    if(operator_id != owner_id_) return RoomResult::NOT_OWNER;
    if(state_ == RoomState::RUNNING) return RoomResult::ALREADY_RUNNING;
    // ensure all players ready
    for(auto pid : players_) {
        if(ready_map_.find(pid) == ready_map_.end() || !ready_map_[pid]) return RoomResult::NOT_READY;
    }
    state_ = RoomState::RUNNING;
    // broadcast start
    broadcast("GAME_STARTED");
    return RoomResult::OK;
}

void Room::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lk(mu_);
    for(auto pid : players_) {
        if(sendFunc_) sendFunc_(pid, message);
    }
}

void Room::sendTo(uint64_t player_id, const std::string& message) {
    if(sendFunc_) sendFunc_(player_id, message);
}

bool Room::containsPlayer(uint64_t player_id) {
    std::lock_guard<std::mutex> lk(mu_);
    return std::find(players_.begin(), players_.end(), player_id) != players_.end();
}

void Room::ensure_owner_after_leave_locked() {
    if(players_.empty()) {
        owner_id_ = 0;
        return;
    }
    if(std::find(players_.begin(), players_.end(), owner_id_) == players_.end()) {
        // pick first as new owner
        owner_id_ = players_.front();
    }
}
