#pragma once
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>
#include <string>

#include "Player.hpp"
#include "ClientSession.hpp"

class PlayerManager {
public:
    PlayerManager() = default;
    ~PlayerManager() = default;

    void OnLoginSuccess(std::shared_ptr<ClientSession> session, int userId);
    void LogOut(std::shared_ptr<ClientSession> session);

    // 将玩家绑定到房间（roomId = 0 表示大厅）
    void setPlayerRoom(std::shared_ptr<Player> player, uint64_t roomId);
    
    std::shared_ptr<Player> getPlayerById(int userId);

    //测试接口
    std::shared_ptr<Player> LoadPlayerFromDB(int userId) {
        // 模拟从数据库加载玩家数据
        auto player = std::make_shared<Player>();
        player->PlayerId = userId;
        player->name = "Player" + std::to_string(userId);
        return player;
    }
private:

    PlayerManager(const PlayerManager&) = delete;
    PlayerManager& operator=(const PlayerManager&) = delete;

    std::unordered_map<int, std::shared_ptr<Player>> idToPlayerMap_;
    std::shared_mutex mutex_;
};
