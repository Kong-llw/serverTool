#include <iostream>

#include "JSONTranslator.hpp"
#include "SessionManager.hpp"
using tcp = asio::ip::tcp;

SessionManager::SessionManager(asio::any_io_executor exec,MessageDispatcher* dispatcher)
    : executor_(exec), heartbeat_timer_(exec), dispatcher_(dispatcher) {
    start_heartbeat();
}

SessionManager::~SessionManager() = default;

uint64_t SessionManager::CreateSession(tcp::socket&& _socket) {
    std::unique_lock<std::shared_mutex> write_lock(rw_mutex_);
    uint64_t new_id = sessionId_.fetch_add(1);
    asio::socket_base::keep_alive option(true);
    _socket.set_option(option);

    auto conn = std::make_shared<TcpConnection>(std::move(_socket), new_id);
    std::shared_ptr<ClientSession> session = std::make_shared<ClientSession>(conn);

    session->setMsgHandler([this](const FrameProtocolCodec::DecodedPacket& pkt, const std::shared_ptr<ClientSession> senderSession) {
        if(this->dispatcher_) this->dispatcher_->ProcessMessagePkt(pkt, senderSession);
    });
    this->dispatcher_->Login(session, userId++); // TODO: 应该是
    session->start();
    activeSessions_.emplace(new_id, session);
    return new_id;
}

int SessionManager::DeleteSession(uint64_t _sessionId){
    int result = 0;
    {
        std::unique_lock<std::shared_mutex> write_lock(rw_mutex_);
        auto iter = activeSessions_.find(_sessionId);
        if(iter != activeSessions_.end()){
            iter->second->closeConnection();
        }
        result = activeSessions_.erase(_sessionId);
    }

    if (_onSessionClose) {
        _onSessionClose(activeSessions_[_sessionId]);
    }
    return result;
}



int SessionManager::BroadCastMsg(const std::string& _msg){
    std::shared_lock<std::shared_mutex> read_lock(rw_mutex_);
    std::vector<uint64_t> IdToDelete;
    for(auto i : activeSessions_){
        if(i.second != nullptr){
            i.second->sendMessage(_msg);
        } else{
            IdToDelete.push_back(i.first);
        }
    }

    if(!IdToDelete.empty())
    {
        asio::post(executor_, [this, IdToDelete](){
            for(auto id : IdToDelete){
                this->DeleteSession(id);
            }
        });
    }
    return 0;
}

bool SessionManager::isConnectionFull() {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    return activeSessions_.size() >= MAX_CONNECTION_NUM;
}

void SessionManager::start_heartbeat(){
    heartbeat_timer_.expires_after(HEARTBEAT_INTERVAL);
    heartbeat_timer_.async_wait(asio::bind_executor(executor_, [this](std::error_code ec){
        if(!ec) on_heartbeat();
    }));
}

void SessionManager::on_heartbeat(){
    std::vector<uint64_t> IdToDelete;
    {
        std::shared_lock<std::shared_mutex> read_lock(rw_mutex_);
        for(auto &[id, session] : activeSessions_){
            if(session){
                session->sendHeartBeat();
            } else {
                IdToDelete.push_back(id);
            }
        }
    }

    for(auto id : IdToDelete){
        DeleteSession(id);
    }

    start_heartbeat();
}


