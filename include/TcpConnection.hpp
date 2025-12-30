#ifndef TCP_CONNECTION_HPP
#define TCP_CONNECTION_HPP

#include <iostream>
#include <queue>
#include <vector>
#include <functional>
#include <memory>
#include <asio.hpp>

using tcp = asio::ip::tcp;

struct ClientMsg{
    uint64_t session_id;
    uint16_t client_port;
    std::string client_ip;
    std::string&& msg_content;
};

class TcpConnection : public std::enable_shared_from_this<TcpConnection>{
public:
    using CloseCallback = std::function<void(u_int64_t)>;
    using DataCallback = std::function<void(const char*, size_t)>;
    using MsgReportCallback = std::function<void(const ClientMsg&)>;

    explicit TcpConnection(tcp::socket socket, u_int64_t conn_id)
        : socket_(std::move(socket)), strand_(asio::make_strand(socket_.get_executor())), connId_(conn_id) {}

    void setCallbacks(CloseCallback cb_close, DataCallback cb_data) {
        onDisconnect_ = cb_close;
        onDataRcv_ = cb_data;
        //onConnect_ = cb_connect
    }

    void startRead() {
        auto self = shared_from_this();
        socket_.async_read_some(asio::buffer(read_buffer_),
            asio::bind_executor(strand_, [self](std::error_code ec, size_t bytesGot){
                if(!ec){
                    if(self->onDataRcv_){
                        self->onDataRcv_(self->read_buffer_.data(), bytesGot);
                    }
                    self->startRead();
                }else{
                    self->close();
                }
            }));
    }

    void sendBytes(std::vector<char> data) {
        auto self = shared_from_this();
        asio::post(strand_, [self, d = std::move(data)](){
            bool writing = !self->writeQ_.empty();
            self->writeQ_.push(std::move(d));
            if(!writing) self->doWrite();
        });
    }

    void close(){
        if(socket_.is_open()){
            asio::error_code ec;
            socket_.close(ec);
            if (onDisconnect_) onDisconnect_(connId_);
        }
    }

    bool isConnected(){
        return socket_.is_open() && !socket_.non_blocking();
    }
    const tcp::socket& getSocket() const {return socket_;}
    
private:
    void doWrite(){
        auto self = shared_from_this();
        if(writeQ_.empty()) return;

        const auto& data = writeQ_.front();
        asio::async_write(socket_, asio::buffer(data),asio::bind_executor(strand_,
            [self](std::error_code ec, size_t len){
                if(!ec){
                    self->writeQ_.pop();
                    if(!self->writeQ_.empty())
                        self->doWrite();
                }else{
                    std::cerr << "Write Error" << ec.message() << std::endl;
                    self->close();
                }
            }));
    }

    tcp::socket socket_;
    asio::strand<asio::any_io_executor> strand_;
    uint64_t connId_;
    std::array<char, 4096> read_buffer_;
    std::queue<std::vector<char>> writeQ_;

    CloseCallback onDisconnect_;
    DataCallback onDataRcv_;
    //ConnectCallback onConnect_;
};

#endif