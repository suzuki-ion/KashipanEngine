#pragma once
#include <cstdint>
#include <string>
#include <random>
#include <sstream>
#include <iomanip>

namespace KashipanEngine {

// UUIDクラス
class UUID128 {
private:
    uint64_t high_; // 上位64ビット
    uint64_t low_;  // 下位64ビット
    std::string stringUUID_;

    void GenerateUUID() {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        static std::uniform_int_distribution<int> dis(0, 15);
        static std::uniform_int_distribution<int> dis2(8, 11);

        std::stringstream ss;
        ss << std::hex << std::setfill('0');

        for (int i = 0; i < 8; i++) ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 4; i++) ss << dis(gen);
        ss << "-4"; // バージョン4を指定
        for (int i = 0; i < 3; i++) ss << dis(gen);
        ss << "-";
        ss << dis2(gen); // バリアントを指定
        for (int i = 0; i < 3; i++) ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 12; i++) ss << dis(gen);

        stringUUID_ = ss.str();

        // ハイフンを取り除いた32文字の16進数文字列を作る
        std::string hexStr;
        for (char c : stringUUID_) {
            if (c != '-') hexStr += c;
        }

        if (hexStr.length() == 32) {
            // 前半16文字をhigh、後半16文字をlowに変換
            high_ = std::stoull(hexStr.substr(0, 16), nullptr, 16);
            low_ = std::stoull(hexStr.substr(16, 16), nullptr, 16);
        }
    }

public:
    UUID128() { GenerateUUID(); }
    ~UUID128() = default;

    void ReGenerate() { GenerateUUID(); }
    bool operator==(const UUID128 &other) const { return high_ == other.high_ && low_ == other.low_; }
    bool operator!=(const UUID128 &other) const { return !(*this == other); }
    const std::string &ToString() const { return stringUUID_; }
    uint64_t GetHigh() const { return high_; }
    uint64_t GetLow() const { return low_; }
};

} // namespace KashipanEngine