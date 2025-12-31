#include <iostream>
#include <asio.hpp>
#include <string>
#include <thread>
#include "FrameProtocalCodec.hpp"
#include "TcpConnection.hpp"
#include <ctime>

using tcp = asio::ip::tcp;
#define TEST_PORT 20003
#define SERVER_PORT 20002
class TcpClient {
public:
    TcpClient(asio::io_context& io_context, const std::string& server_ip, uint16_t server_port)
        : io_context_(io_context), server_endpoint_(asio::ip::make_address(server_ip), server_port){

        codec_ = std::make_unique<FrameProtocolCodec>();
        
    }
    //发消息到server
    void start() {
        auto socket = std::make_shared<tcp::socket>(io_context_);
        socket->async_connect(server_endpoint_,
            [this, socket](const asio::error_code& ec) {
                if (!ec) {
                    //std::cout << "Connected to server." << std::endl;
                    LOG_INFO("Connected to server.");
                    connection_ = std::make_shared<TcpConnection>(TcpConnection(std::move(*socket), 0));
                    connection_->setCallbacks(
                        [this](uint64_t id) {onConnectionClose(id);},
                        [this](const char* data, size_t len) {onRawDataReceived(data,len);}
                    );
                    connection_->startRead();

                    std::thread input_thread(&TcpClient::read_user_input, this);
                    input_thread.detach();
                } else {
                    std::cerr << "Failed to connect: " << ec.message() << std::endl;
                    io_context_.stop();
                }
            });
    }

    void sendMessage(const std::string& msg) {
        if (!connection_ || !connection_->isConnected()) {
            std::cerr << "❌ Send failed: connection not ready!" << std::endl;
            return;
        }
        // ✅ 用编解码器打包，再通过TcpConnection发送（和服务端一模一样的逻辑）
        auto frames = codec_->encode(sentMsgUid_++, msg);
        for (auto& frame : frames) {
            connection_->sendBytes(std::move(frame.data));
        }
        //std::cout << "Send to server: " << msg << std::endl;
    }
private:
    void onConnectionClose(uint64_t id){
        connection_->close();
    }

    void onRawDataReceived(const char* data, size_t len){
        std::vector<FrameProtocolCodec::DecodedPacket> messages;
        codec_->decode(data, len, messages);
        //处理解析后的内容
        for (const auto& msg : messages){
            time_t t = time(nullptr);
            std::cout << std::ctime(&t) << msg.body << std::endl;
        }
    }

    void read_user_input() {
        std::string user_input;
        while (true) {
            std::getline(std::cin, user_input);
            if (user_input == "exit") {
                io_context_.stop();
                break;
            }
            if(!user_input.empty())
                sendMessage(std::move(user_input));
        }
    }


    asio::io_context& io_context_;
    tcp::endpoint server_endpoint_;
    std::shared_ptr<TcpConnection> connection_;
    std::unique_ptr<FrameProtocolCodec> codec_;
    uint64_t sentMsgUid_ = 0;
};

int main(int argc, char* argv[])
{
    try {
        /*
        if (argc != 3) {
            std::cerr << "Usage: TcpClient <server_ip> <server_port>\n";
            return 1;
        }
        */ 

        std::string server_ip = "43.139.229.163";
        const uint16_t server_port = SERVER_PORT;

        asio::io_context io_context;

        TcpClient client(io_context, server_ip, server_port);
        client.start();

        io_context.run();
    } catch (const std::exception& e){
        std::cerr << "Client Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}