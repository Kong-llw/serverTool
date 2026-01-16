#ifndef JSON_TRANSLATOR_H
#define JSON_TRANSLATOR_H

#include <cstdint>
#include <string>
#include "json.hpp"
#include "Room.hpp"

enum GameCmdType : uint8_t {
    GAMECMD_MOVE = 0,
    GAMECMD_ATTACK = 1,
    GAMECMD_GROW = 2,
    GAMECMD_ENDTURN = 3,
    GAMECMD_JSONCOMMAND = 4,

    GAMEDATA_ROOMLIST = 50,
    
    GAMESTART = 100,
    MAPSYNC = 101,
    PLAYERJOIN = 102,
    PLAYERLEAVE = 103,
}; 

struct JsonCommand {
    uint8_t cmd_type; //对应 GameCmdType
    std::string cmd_data; // JSON 格式的命令数据
};

struct JsonMoveCommand {
    uint32_t source_id;
    uint32_t dest_id;
    uint32_t value;
};

struct JsonAttackCommand {
    uint32_t source_id;
    uint32_t dest_id;
    uint32_t source_value;
    uint32_t dest_value;
};

struct JsonGrowCommand {
    uint32_t target_tile_id;
    uint32_t grow_value;
};


class JSONTranslator {
public:
    static std::string createGameCommandJSON(GameCmdType cmdType, const std::string& cmdData);
    static void ParseCommand(const std::string& jsonStr);

    static std::string serializeRoomList(const std::vector<RoomInListInfo>& rooms);
    static std::string serializeCreateRoomResult(const std::string& room_code, RoomResult res);
private:
    JSONTranslator() = default;
    ~JSONTranslator() = default;
    JSONTranslator(const JSONTranslator&) = delete;
    JSONTranslator& operator=(const JSONTranslator&) = delete;
};

#endif // JSON_TRANSLATOR_H