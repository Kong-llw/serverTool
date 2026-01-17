#include "PlayerManager.hpp"
#include <mutex>
#include <shared_mutex>

void PlayerManager::OnLoginSuccess(std::shared_ptr<ClientSession> session, int userId) {
    // 1. 创建或加载 Player
    std::shared_ptr<Player> player = LoadPlayerFromDB(userId);
    player->session = session;
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        idToPlayerMap_[userId] = player;
    }

    // 2. 【关键】直接把 Player 塞给 Session
    // 以后 Session 自己就知道自己是谁了
    session->setPlayerData(player);

    // 3. 反向绑定 (Player 也要知道 Session，以便发消息)
    player->session = session;
}

void PlayerManager::LogOut(std::shared_ptr<ClientSession> session) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto player = session->GetPlayer();
    if (player) {
        idToPlayerMap_.erase(player->PlayerId);
    }
}

void PlayerManager::setPlayerRoom(std::shared_ptr<Player> player, uint64_t roomId) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    player->CurrentRoomId = roomId;
}

std::shared_ptr<Player> PlayerManager::getPlayerById(int userId) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = idToPlayerMap_.find(userId);
    if (it == idToPlayerMap_.end()) return nullptr;
    return it->second;
}

