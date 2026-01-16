// RoomUtils.hpp
// 辅助工具：生成房间号与校验函数（小型静态工具类风格）
#pragma once

#include <string>
#include <random>
#include <cctype>

namespace RoomUtils {
    // 生成 6 位数字房间号（字符串）
    inline std::string genRoomCode() {
        static std::mt19937_64 rng((std::random_device())());
        static const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        static std::uniform_int_distribution<int> dist(0, chars.size() - 1);
        std::string s; s.reserve(6);
        for (int i = 0; i < 6; ++i) s.push_back(chars[dist(rng)]);
        return s;
    }

    inline bool isValidRoomCode(const std::string& code) {
        if(code.size() != 6) return false;
        for(char c : code) if(!std::isdigit(static_cast<unsigned char>(c))) return false;
        return true;
    }
}
