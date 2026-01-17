#pragma once

#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <functional>
#include <asio.hpp>

#include "ClientSession.hpp"
#include "MessageDispatcher.hpp"

class SessionManager {
public:
    explicit SessionManager(asio::any_io_executor exec, MessageDispatcher* dispatcher);
    ~SessionManager();

    using SessionCloseHandler = std::function<void(std::shared_ptr<ClientSession>)>;
    void SetOnSessionClose(SessionCloseHandler cb) {
        _onSessionClose = cb;
    }

    const int MAX_CONNECTION_NUM = 1000;
    uint64_t CreateSession(asio::ip::tcp::socket&& _socket);
    int DeleteSession(uint64_t _sessionId);

    // Message processing is delegated to MessageDispatcher

    int BroadCastMsg(const std::string& _msg);
    int BroadCastRoomMsg(uint64_t room_id, const std::string& msg);

    bool isConnectionFull();

    template <typename Callable> requires std::is_invocable_r_v<void, Callable, std::shared_ptr<ClientSession>>
    void forEachSession(Callable&& func) {
        std::shared_lock<std::shared_mutex> lock(rw_mutex_);
        for (auto& [sid, session] : activeSessions_) {
            func(session);
        }
    }
private:
//Test:
    std::atomic<int> userId{1};
    // non-copyable
    SessionManager() = delete;
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    void start_heartbeat();
    void on_heartbeat();
    SessionCloseHandler _onSessionClose;
    MessageDispatcher* dispatcher_;

    std::unordered_map<uint64_t, std::shared_ptr<ClientSession>> activeSessions_;
    std::atomic<uint64_t> sessionId_ = 0;  
    std::shared_mutex rw_mutex_;
    asio::any_io_executor executor_;
    asio::steady_timer heartbeat_timer_;
    const std::chrono::seconds HEARTBEAT_INTERVAL{5};
};
