#include "JSONTranslator.hpp"
#include <iostream>

using nlohmann::json;

std::string JSONTranslator::createGameCommandJSON(GameCmdType cmdType, const std::string& cmdData) {
    return R"({"cmd_type":)" + std::to_string(static_cast<uint8_t>(cmdType)) + R"(,"cmd_data":)" + cmdData + R"(})";
}

void JSONTranslator::ParseCommand(const std::string& jsonStr) {
    try {
        json root = json::parse(jsonStr);

        if(!root.contains("cmd_type")){
            std::cerr << "ParseCommand: missing cmd_type" << std::endl;
            return;
        }

        uint8_t cmd_type = 0;
        try{
            cmd_type = static_cast<uint8_t>(root.at("cmd_type").get<int>());
        } catch(...) {
            std::cerr << "ParseCommand: invalid cmd_type format" << std::endl;
            return;
        }

        std::string cmd_data_str;
        if(root.contains("cmd_data")){
            // cmd_data may be object or string; keep its JSON text
            if(root["cmd_data"].is_string()){
                cmd_data_str = root["cmd_data"].get<std::string>();
            } else {
                cmd_data_str = root["cmd_data"].dump();
            }
        }

        JsonCommand pkt;
        pkt.cmd_type = cmd_type;
        pkt.cmd_data = cmd_data_str;

        switch(static_cast<GameCmdType>(pkt.cmd_type)){
            case GAMECMD_MOVE: {
                try{
                    json d = json::parse(pkt.cmd_data);
                    JsonMoveCommand mv{};
                    mv.source_id = d.value("source_id", 0u);
                    mv.dest_id = d.value("dest_id", 0u);
                    mv.value = d.value("value", 0u);
                    std::cout << "Parsed MOVE: src=" << mv.source_id << " dst=" << mv.dest_id << " val=" << mv.value << std::endl;
                } catch(const std::exception& e){
                    std::cerr << "ParseCommand: invalid MOVE data: " << e.what() << std::endl;
                }
                break;
            }
            case GAMECMD_ATTACK: {
                try{
                    json d = json::parse(pkt.cmd_data);
                    JsonAttackCommand at{};
                    at.source_id = d.value("source_id", 0u);
                    at.dest_id = d.value("dest_id", 0u);
                    at.source_value = d.value("source_value", 0u);
                    at.dest_value = d.value("dest_value", 0u);
                    std::cout << "Parsed ATTACK: src=" << at.source_id << " dst=" << at.dest_id << " sVal=" << at.source_value << " dVal=" << at.dest_value << std::endl;
                } catch(const std::exception& e){
                    std::cerr << "ParseCommand: invalid ATTACK data: " << e.what() << std::endl;
                }
                break;
            }
            case GAMECMD_GROW: {
                try{
                    json d = json::parse(pkt.cmd_data);
                    JsonGrowCommand gr{};
                    gr.target_tile_id = d.value("target_tile_id", 0u);
                    gr.grow_value = d.value("grow_value", 0u);
                    std::cout << "Parsed GROW: tile=" << gr.target_tile_id << " grow=" << gr.grow_value << std::endl;
                } catch(const std::exception& e){
                    std::cerr << "ParseCommand: invalid GROW data: " << e.what() << std::endl;
                }
                break;
            }
            default:
                std::cout << "ParseCommand: unhandled cmd_type=" << int(pkt.cmd_type) << " data=" << pkt.cmd_data << std::endl;
        }

    } catch(const json::parse_error& e) {
        std::cerr << "ParseCommand: JSON parse error: " << e.what() << std::endl;
    } catch(const std::exception& e){
        std::cerr << "ParseCommand: exception: " << e.what() << std::endl;
    }
}

std::string JSONTranslator::serializeRoomList(const std::vector<RoomInListInfo>& rooms) {
    json root;
    root["cmd_type"] = GAMEDATA_ROOMLIST;
    root["rooms"] = json::array();
    auto& roomArray = root["rooms"];
    json r;
    for(const auto& room : rooms){
        r.clear();
        r["room_code"] = room.room_code;
        r["room_name"] = room.room_name;
        r["capacity"] = room.capacity;
        r["player_count"] = room.player_count;
        r["state"] = static_cast<uint8_t>(room.state);
        root["rooms"].emplace_back(std::move(r));
    }
    return root.dump();
}

std::string JSONTranslator::serializeCreateRoomResult(const std::string& room_code, RoomResult res) {
    json root;
    root["room_code"] = room_code;
    root["result"] = static_cast<int>(res);
    return root.dump();
}