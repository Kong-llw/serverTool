#include <iostream>
#include <asio.hpp>
#include <string>
#include <memory>
#include <atomic>
#include <shared_mutex>
#include <vector>
#include <unordered_map>
#include <queue>
#include <chrono>

#include "TcpConnection.hpp"
#include "FrameProtocalCodec.hpp"

using tcp = asio::ip::tcp;
#define SERVER_PORT 20002

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    explicit ClientSession(std::shared_ptr<TcpConnection> conn):connection_(conn){
        codec_ = std::make_unique<FrameProtocolCodec>();
        connection_->setCallbacks(
            [this](uint64_t id) {onConnectionClose(id);},
            [this](const char* data, size_t len) {onRawDataReceived(data,len);}
        );
    }
    using MsgHandler = std::function<void(const FrameProtocolCodec::DecodedPacket&)>;
    void setMsgHandler(MsgHandler handler) {
        msgHandler_ = std::move(handler);
    }

    void sendMessage(const std::string& msg){
        auto frames = codec_->encode(sentMsgUid_++, msg);
        for(auto& frame : frames){
            connection_->sendBytes(std::move(frame.data));
        }
    }

    void sendHeartBeat(){
        auto frame = codec_->createHeartbeatFrame();
        connection_->sendBytes(std::move(frame.data));
    }

    void start() {
        connection_->startRead();

        tcp::endpoint&& client_endpoint = connection_->getSocket().remote_endpoint();

        std::cout << "Client IP " << client_endpoint.address().to_string() << "connected on port " << client_endpoint.port() << std::endl;
        std::string server_msg = "ServerConnected. IP: " + connection_->getSocket().local_endpoint().address().to_string() 
                                    + " Port: " + std::to_string(SERVER_PORT) + "\n";
        
        sendMessage(server_msg);
    }

    void closeConnection(){
        connection_->close();
    }

private:
    void onRawDataReceived(const char* data, size_t len){
        std::vector<FrameProtocolCodec::DecodedPacket> messages;
        codec_->decode(data, len, messages);
        //处理解析后的内容
        for (const auto& msg : messages){
            if(msg.ptype == ProtoInfo::HEARTBEAT){
                //做一些别的处理
                continue;
            }
            msgHandler_(msg);
        }
    }

    void onConnectionClose(uint64_t id){
        //connection_->close();
    }

    
    uint64_t sentMsgUid_ = 0;
    std::shared_ptr<TcpConnection> connection_;
    std::unique_ptr<FrameProtocolCodec> codec_;

    MsgHandler msgHandler_; //数据包处理回调 Manager注入
};

class SessionManager{
public:
    explicit SessionManager(asio::any_io_executor exec)
        : executor_(exec), heartbeat_timer_(exec) {
        start_heartbeat();
    }
    ~SessionManager() = default;

    const int MAX_CONNECTION_NUM = 1000;
    uint64_t CreateSession(tcp::socket&& _socket) {
        std::unique_lock<std::shared_mutex> write_lock(rw_mutex_);
        uint64_t new_id = sessionId_.fetch_add(1);
        asio::socket_base::keep_alive option(true);
        _socket.set_option(option);

        auto conn = std::make_shared<TcpConnection>(std::move(_socket), new_id);
        std::shared_ptr<ClientSession> session = std::make_shared<ClientSession>(conn);
        
        session->setMsgHandler([this](const FrameProtocolCodec::DecodedPacket& pkt) {
            this->DispatchMessagePkg(pkt);
        });

        session->start();
        activeSessions_.emplace(new_id, session);
        return new_id;
    }
    
    int DeleteSession(uint64_t _sessionId){
        std::unique_lock<std::shared_mutex> write_lock(rw_mutex_);
        auto iter = activeSessions_.find(_sessionId);
        if(iter != activeSessions_.end()){
            // ✅ 主动关闭连接，释放资源
            iter->second->closeConnection();
        }
        int result =  activeSessions_.erase(_sessionId);
        return result == 0 ? 1 : 0;
    }
    //DispatchMessage和windows函数重名
    void DispatchMessagePkg(const FrameProtocolCodec::DecodedPacket& pkt) {
        // 处理聊天，广播给所有人 (排除自己或不排除)
        //std::string chatContent = "User[" + std::to_string(senderId) + "] says: " + pkt.body;
        std::cout << "DispatchMessagePkg Got Message: " << pkt.body << std::endl;
        BroadCastMsg(pkt.body); // 传入 senderId 以便排除自己
    }

    int BroadCastMsg(const std::string& _msg){
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

    bool isConnectionFull() {
        std::shared_lock<std::shared_mutex> lock(rw_mutex_);
        return activeSessions_.size() >= MAX_CONNECTION_NUM;
    }

private:
    //心跳函数
    std::unordered_map<uint64_t, std::shared_ptr<ClientSession>> activeSessions_;
    std::atomic<uint64_t> sessionId_ = 0;  
    std::shared_mutex rw_mutex_;
    asio::any_io_executor executor_;
    asio::steady_timer heartbeat_timer_;
    const std::chrono::seconds HEARTBEAT_INTERVAL{5};

    void start_heartbeat(){
        heartbeat_timer_.expires_after(HEARTBEAT_INTERVAL);
        heartbeat_timer_.async_wait(asio::bind_executor(executor_, [this](std::error_code ec){
            if(!ec) on_heartbeat();
        }));
    }

    void on_heartbeat(){
        std::vector<uint64_t> IdToDelete;
        {
            std::shared_lock<std::shared_mutex> read_lock(rw_mutex_);
            for(auto &[id, session] : activeSessions_){
                if(session){
                    // 发送轻量心跳，可自定义格式
                    session->sendHeartBeat();
                } else {
                    IdToDelete.push_back(id);
                }
            }
        }

        // 删除失效会话（DeleteSession 内部拿写锁）
        for(auto id : IdToDelete){
            DeleteSession(id);
        }

        // 继续下一次定时
        start_heartbeat();
    }

    SessionManager() = delete;
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;
};


class TcpServer {
public:
    TcpServer(asio::io_context& io_context)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), SERVER_PORT)),
          io_context_(io_context),
          session_manager_(io_context.get_executor()) {
            accept_connections();
    }

private:
    void accept_connections() {
        acceptor_.async_accept(
            [this](std::error_code ec, tcp::socket socket){
                if (!ec) {
                    if(session_manager_.isConnectionFull()){
                        std::cerr << "Server full! Reject connection: " << socket.remote_endpoint().address().to_string() << std::endl;
                        socket.close();
                    }else{
                        int ssId = session_manager_.CreateSession(std::move(socket));
                        std::cout << "Connect In SessionId:" << ssId << std::endl;
                    }

                }
                else{
                    std::cerr << "Accept error: " << ec.message() << std::endl;
                }

                accept_connections();
            });
    }

    tcp::acceptor acceptor_;
    asio::io_context& io_context_;
    SessionManager session_manager_;
};

int main()
{
    try {
        asio::io_context io_context;

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](std::error_code, int){
            std::cout<< "Signal recevied. Stopping server..." << std::endl;
            io_context.stop();
        });

        TcpServer server(io_context);
        std::cout << "Server is running on port " << SERVER_PORT << std::endl;
        io_context.run();

        std::cout << "Server stopped (io_context exited)" << std::endl;
    }
    catch (std::exception& e) {
        std::cerr << "Main Exception: " << e.what() << std::endl;
        return 1;
    }

    LOG_INFO("Server stoped normally.");
    return 0;
}
