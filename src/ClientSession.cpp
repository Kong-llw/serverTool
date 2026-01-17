#include "ClientSession.hpp"
#include <iostream>
#include <asio.hpp>

using tcp = asio::ip::tcp;

ClientSession::ClientSession(std::shared_ptr<TcpConnection> conn)
    : connection_(conn) {
    codec_ = std::make_unique<FrameProtocolCodec>();
    connection_->setCallbacks(
        [this](uint64_t id) { onConnectionClose(id); },
        [this](const char* data, size_t len) { onRawDataReceived(data, len); }
    );
}

void ClientSession::setMsgHandler(MsgHandler handler) {
    msgHandler_ = std::move(handler);
}

void ClientSession::sendMessage(const std::string& msg, ProtoInfo::ProtocolType ptype){
    auto frames = codec_->encode(sentMsgUid_++, msg, ptype);
    for(auto& frame : frames){
        connection_->sendBytes(std::move(frame.data));
    }
}

void ClientSession::sendHeartBeat(){
    auto frame = codec_->createHeartbeatFrame();
    connection_->sendBytes(std::move(frame.data));
}

void ClientSession::start() {
    connection_->startRead();

    tcp::endpoint&& client_endpoint = connection_->getSocket().remote_endpoint();

    std::cout << "Client IP " << client_endpoint.address().to_string() << "connected on port " << client_endpoint.port() << std::endl;
    std::string server_msg = "ServerConnected. IP: " + connection_->getSocket().local_endpoint().address().to_string()
                                + " Port: " + std::to_string(20002) + "\n";
    
    sendMessage(server_msg);
}

uint64_t ClientSession::getId() const {
    return connection_->getId();
}

void ClientSession::closeConnection(){
    connection_->close();
}

void ClientSession::onRawDataReceived(const char* data, size_t len){
    std::vector<FrameProtocolCodec::DecodedPacket> messages;
    codec_->decode(data, len, messages);
    for (const auto& msg : messages){
        if(msg.info.ptype == ProtoInfo::HEARTBEAT){
            continue;
        }
        connection_->post_to_strand([self = shared_from_this(), msg = std::move(msg)]() mutable 
        {
            if(self->msgHandler_)
                self->msgHandler_(std::move(msg), self);
        });
    }
}

void ClientSession::onConnectionClose(uint64_t id){
    // connection_->close(); // handled elsewhere
}
