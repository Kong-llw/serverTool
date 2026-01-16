#pragma once

#include <arpa/inet.h>
#include <string>

#include "RoomManager.hpp"
#include "proto.h"

void ParseRoomReq(const std::string& req){
    if(req.empty()) return;
    //解析房间请求
    int offset = 0;
    uint8_t reqType = req[0];
    offset++;
    switch(reqType){
        case ProtoInfo::RoomReqType::CREATEROOM: //创建房间
        {
            createRoomInfo info;
            int length = ntohs(*(uint16_t*)(req.data() + offset));
            offset += 2;
            info.room_name = req.substr(offset, length);
            offset += length;
            length = ntohs(*(uint16_t*)(req.data() + offset));
            offset += 2;
            info.passwd = req.substr(offset, length);
            break;
        }

        case ProtoInfo::RoomReqType::JOINROOM: //加入房间
            break;
        case ProtoInfo::RoomReqType::LEAVEROOM: //离开房间
            break;
        case ProtoInfo::RoomReqType::LISTROOMS: //离开房间
            break;
        case ProtoInfo::RoomReqType::SETREADY: //离开房间
            break;
        case ProtoInfo::RoomReqType::SETCAPACITY: //离开房间
            break;
        case ProtoInfo::RoomReqType::STARTGAME: //离开房间
            break;
        case ProtoInfo::RoomReqType::DISSOLVEROOM: //离开房间
            break;
        default:
            std::cerr << "Unknown Room Request Type: " << static_cast<int>(reqType) << std::endl;
            return;
    }
};