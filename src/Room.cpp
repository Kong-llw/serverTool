// Room.cpp
#include "Room.hpp"
#include <algorithm>
#include <iostream>

Room::Room(uint64_t id, const std::shared_ptr<Player>& owner, uint32_t capacity, const std::string& room_name,
     const std::string& room_code, const std::string& password, SendCallback sender)
    : id_(id), room_name_(room_name), room_code_(room_code), password_(password) ,
     capacity_(capacity), state_(RoomState::LOBBY), sendFunc_(std::move(sender)) {
        join(owner);
}

Room::~Room() {
}

/*
RoomInfo Room::getAllInfo() {
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
        //.player_infos = players_
    };
}*/

RoomInListInfo Room::getInListInfo() {
    std::lock_guard<std::mutex> lk(mu_);
    return RoomInListInfo{
        .room_code = room_code_, 
        .room_name = room_name_,
        .capacity = capacity_,
        .player_count = players_.size(),
        .state = state_ };
}

const std::vector<std::shared_ptr<Player>>& Room::getPlayers() {
    std::lock_guard<std::mutex> lk(mu_);
    return players_;
}

/*std::vector<PlayerInfoInRoom> Room::getPlayerInfos() {
    std::lock_guard<std::mutex> lk(mu_);
    
    std::vector<PlayerInfoInRoom> infos;
    for(size_t i = 0; i < players_.size(); ++i) {
        PlayerInfoInRoom info;
        info.player_id = players_[i].player_id;
        info.seat_index = players_[i].seat_index;
        info.is_owner = (players_[i].player_id == owner_id_);
        info.ready = players_[i].ready;
        // other fields can be filled as needed
        infos.push_back(info);
    }
    return players_;
}*/

RoomResult Room::join(const std::shared_ptr<Player>& pplayer) {
    std::lock_guard<std::mutex> lk(mu_);
    if(state_ != RoomState::LOBBY) return RoomResult::ALREADY_RUNNING;
    if(players_.size() >= capacity_) return RoomResult::FULL;
    for(auto& p : players_) {
        if(p == pplayer) return RoomResult::ALREADY_IN_ROOM;
    }

    players_.push_back(pplayer);
    // notify join
    return RoomResult::OK;
}

bool Room::roomIsEmpty() {
    std::lock_guard<std::mutex> lk(mu_);
    return players_.empty();
}

RoomResult Room::leave(const std::shared_ptr<Player>& pPlayer) {
    std::lock_guard<std::mutex> lk(mu_);

    RoomResult res = RoomResult::NOT_IN_ROOM;
    for (auto it = players_.begin(); it != players_.end(); ++it) {
        if (*it == pPlayer) {
            players_.erase(it); // erase传迭代器，正确删除元素
            res = RoomResult::OK;
            break; // 找到并删除后立即退出循环，无需继续遍历
        }
    }
    if (res != RoomResult::OK) return res;

    // adjust owner if needed
    ensure_owner_after_leave_locked();
    // notify
    if(sendFunc_) sendFunc_(pPlayer->PlayerId, "LEFT_ROOM");
    return RoomResult::OK;
}

RoomResult Room::setReady(const std::shared_ptr<Player>& pPlayer, bool ready) {
    std::lock_guard<std::mutex> lk(mu_);
    for(auto& p : players_) {
        if(p == pPlayer) {
            p->RoomData.ready = ready;
            return RoomResult::OK;
        }
    }
    return RoomResult::NOT_IN_ROOM;
}

bool Room::isRoomOwner(const std::shared_ptr<Player>& pPlayer) {
    std::lock_guard<std::mutex> lk(mu_);
    return owner == pPlayer;
}

RoomResult Room::setCapacity(const std::shared_ptr<Player>& pPlayer, uint32_t newcap) {
    std::lock_guard<std::mutex> lk(mu_);
    if(!isRoomOwner(pPlayer)) return RoomResult::NOT_OWNER;
    if(newcap < 2 || newcap > 16) return RoomResult::INVALID_CAPACITY;
    if(newcap < players_.size()) return RoomResult::INVALID_CAPACITY;
    capacity_ = newcap;
    return RoomResult::OK;
}

RoomResult Room::startGame(const std::shared_ptr<Player>& pPlayer) {
    std::lock_guard<std::mutex> lk(mu_);
    if(!isRoomOwner(pPlayer)) return RoomResult::NOT_OWNER;
    if(state_ == RoomState::RUNNING) return RoomResult::ALREADY_RUNNING;
    // ensure all players ready
    for(auto ply : players_) {
        if(!ply->RoomData.ready)
            return RoomResult::NOT_READY;
    }
    state_ = RoomState::RUNNING;
    // broadcast start
    broadcast("GAME_STARTED");
    return RoomResult::OK;
}

void Room::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lk(mu_);
    for(auto player : players_) {
        player->CallSend(message);
    }
}

void Room::sendTo(uint64_t playerId, const std::string& message) {
    std::lock_guard<std::mutex> lk(mu_);
    auto pPlayer = GetPlayer(playerId);
    if(pPlayer) {
        pPlayer->CallSend(message);
    }
}

std::shared_ptr<Player> Room::GetPlayer(uint64_t playerId) {
    std::lock_guard<std::mutex> lk(mu_);
    for(auto& p : players_) {
        if(p->PlayerId == playerId) return p;
    }
    return nullptr;
}

bool Room::containsPlayer(const std::shared_ptr<Player>& pPlayer) {
    std::lock_guard<std::mutex> lk(mu_);
    for(auto& p : players_) {
        if(p == pPlayer) return true;
    }
    return false;
}

void Room::ensure_owner_after_leave_locked() {
    if(players_.empty()) {
        owner = nullptr;
        return;
    }
    if(!containsPlayer(owner)) {
        // pick first as new owner
        owner = players_.front();
    }
}
