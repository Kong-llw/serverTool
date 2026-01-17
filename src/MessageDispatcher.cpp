#include "MessageDispatcher.hpp"
#include <iostream>
#include "JSONTranslator.hpp"
#include "PlayerManager.hpp"

using tcp = asio::ip::tcp;

MessageDispatcher::MessageDispatcher(asio::any_io_executor exec, RoomManager* rm, PlayerManager* pm)
    : executor_(exec), roomManager_(rm), playerManager_(pm){}


bool MessageDispatcher::Login(std::shared_ptr<ClientSession> session, int userId){
    playerManager_->OnLoginSuccess(session, userId);
    return true;
}
void MessageDispatcher::ProcessMessagePkt(const FrameProtocolCodec::DecodedPacket& pkt, const std::shared_ptr<ClientSession>& senderSession){
    std::cout << "DispatchMessagePkg Got Message: " << pkt.body << std::endl;
    switch(pkt.info.ptype){
        case ProtoInfo::CHATMSG:
            roomManager_->RoomBroadCast(senderSession->GetPlayer()->CurrentRoomId, pkt.body);
            break;
        case ProtoInfo::JSONCOMMAND:
            std::cout << "Received Command: " << pkt.body << std::endl;
            roomManager_->RoomBroadCast(senderSession->GetPlayer()->CurrentRoomId, pkt.body);
            return;
        case ProtoInfo::JSONDATA:
            std::cout << "Received Data: " << pkt.body << std::endl;
            return;
        case ProtoInfo::ROOMREQ:
            ParseRoomReq(pkt.body, senderSession);
            std::cout << "Received Room Request: " << pkt.body << std::endl;
            return;
        default:
            std::cerr << "Unknown Protocol Type: " << static_cast<int>(pkt.info.ptype) << std::endl;
            return;
    }
}

void MessageDispatcher::ParseRoomReq(const std::string& req, const std::shared_ptr<ClientSession>& senderSession){
    if(req.empty()) return;
    int offset = 0;
    RoomResult out_res = RoomResult::UNKNOWN_ERROR;
    uint8_t reqType = req[0];
    offset++;
    switch(reqType){
        case ProtoInfo::RoomReqType::CREATEROOM:{
            int nameLen = req[offset];
            offset++;
            std::string name = req.substr(offset, nameLen);
            offset += nameLen;
            int passwdLen = req[offset];
            offset++;
            std::string passwd = req.substr(offset, passwdLen);
            offset += passwdLen;

            std::string out_room_code;
            RoomInfo temp {
                .room_name = name,
                .password = passwd,
                .owner_id = senderSession->getId(),
                .capacity = 4
            };
            std::shared_ptr<Player> pPlayer = senderSession->GetPlayer();
            roomManager_->createRoom(temp, pPlayer, out_room_code, out_res);

            std::string msg = JSONTranslator::serializeCreateRoomResult(out_room_code, out_res);
            msg.insert(0, sizeof(ProtoInfo::RoomReqType), static_cast<char>(ProtoInfo::RoomReqType::CREATEROOM));
            senderSession->sendMessage(msg, ProtoInfo::ProtocolType::ROOMRSP);
            break;
        }

        case ProtoInfo::RoomReqType::JOINROOM:{
            std::string room_code = req.substr(offset, 6);
            offset += 6;
            uint64_t room_id = roomManager_->getRoomId(room_code);
            std::string msg(1,ProtoInfo::RoomReqType::JOINROOM);
            if(room_id == 0){
                msg.append(1, static_cast<char>(RoomResult::NOT_FOUND));
                senderSession->sendMessage(msg, ProtoInfo::ProtocolType::ROOMRSP);
                break;
            }

            out_res = roomManager_->joinRoom(room_id, senderSession->GetPlayer());
            msg.append(1, static_cast<char>(out_res));
            senderSession->sendMessage(msg, ProtoInfo::ProtocolType::ROOMRSP);
            {
                auto player = senderSession->GetPlayer();
                if (player && out_res == RoomResult::OK) {
                    playerManager_->setPlayerRoom(player, (int)room_id);
                }
            }
            roomManager_->RoomBroadCast(senderSession->GetPlayer()->CurrentRoomId, "JOINED_ROOM");
            break;
        }
        case ProtoInfo::RoomReqType::LEAVEROOM:{
            std::string room_code = req.substr(offset, 6);
            offset += 6;
            uint64_t room_id = roomManager_->getRoomId(room_code);
            if (room_id == 0) break;

            std::shared_ptr<Player> player = senderSession->GetPlayer();
            out_res = roomManager_->leaveRoom(room_id, player);
            {
                if (player && out_res == RoomResult::OK) {
                    playerManager_->setPlayerRoom(player, 0);
                }
            }
            std::string msg(1,ProtoInfo::RoomReqType::LEAVEROOM);
            msg.append(1, static_cast<char>(out_res));
            senderSession->sendMessage(msg, ProtoInfo::ProtocolType::ROOMRSP);
            break;
        }
        case ProtoInfo::RoomReqType::LISTROOMS:{
            auto rooms = roomManager_->listRooms();
            out_res = rooms.empty()? RoomResult::OK : RoomResult::NOT_FOUND;

            std::string msg = JSONTranslator::serializeRoomList(rooms);
            msg.insert(0, sizeof(ProtoInfo::RoomReqType), static_cast<char>(ProtoInfo::RoomReqType::LISTROOMS));
            senderSession->sendMessage(msg, ProtoInfo::ProtocolType::ROOMRSP);
            break;
        }
        case ProtoInfo::RoomReqType::SETREADY:{
            break;
        }
        case ProtoInfo::RoomReqType::SETCAPACITY:{
            break;
        }
        case ProtoInfo::RoomReqType::STARTGAME:{
            break;
        }
        case ProtoInfo::RoomReqType::DISSOLVEROOM:{
            break;
        }
        default:
            std::cerr << "Unknown Room Request Type: " << static_cast<int>(reqType) << std::endl;
            break;
    }

}
