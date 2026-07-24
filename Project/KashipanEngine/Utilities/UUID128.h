#pragma once
#include <charconv>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace KashipanEngine {

// UUIDクラス
class UUID128 {
private:
    uint64_t high_; // 上位64ビット
    uint64_t low_;  // 下位64ビット
    std::string stringUUID_;
    bool isValid_ = false;

    void InitUUID() {
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

        isValid_ = true;
    }

    void StringToUUID(const std::string &uuidStr) {
        if (uuidStr.length() != 36) {
            ClearUUID();
            return;
        }

        std::string hexStr;
        hexStr.reserve(32);
        for (size_t i = 0; i < uuidStr.size(); ++i) {
            const bool isHyphenPosition = i == 8 || i == 13 || i == 18 || i == 23;
            if (isHyphenPosition) {
                if (uuidStr[i] != '-') {
                    ClearUUID();
                    return;
                }
            } else {
                if (uuidStr[i] == '-') {
                    ClearUUID();
                    return;
                }
                hexStr += uuidStr[i];
            }
        }

        uint64_t parsedHigh = 0;
        uint64_t parsedLow = 0;
        const char *const begin = hexStr.data();
        const char *const middle = begin + 16;
        const char *const end = begin + hexStr.size();
        const auto highResult = std::from_chars(begin, middle, parsedHigh, 16);
        const auto lowResult = std::from_chars(middle, end, parsedLow, 16);
        if (highResult.ec != std::errc{} || highResult.ptr != middle ||
            lowResult.ec != std::errc{} || lowResult.ptr != end) {
            ClearUUID();
            return;
        }

        high_ = parsedHigh;
        low_ = parsedLow;
        stringUUID_ = uuidStr;
        isValid_ = true;
    }

    void UUIDToString() {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        ss << std::setw(16) << high_;
        ss << std::setw(16) << low_;
        std::string hexStr = ss.str();
        stringUUID_ = hexStr.substr(0, 8) + "-" + hexStr.substr(8, 4) + "-" + hexStr.substr(12, 4) + "-" + hexStr.substr(16, 4) + "-" + hexStr.substr(20, 12);
        isValid_ = true;
    }

    void ClearUUID() {
        high_ = 0;
        low_ = 0;
        stringUUID_.clear();
        isValid_ = false;
    }

public:
    /// @brief デフォルトコンストラクタ（無効なUUIDを生成）
    explicit UUID128() : high_(0), low_(0), stringUUID_(""), isValid_(false) {}
    /// @brief コンストラクタ（新しいUUIDを生成）
    explicit UUID128(bool generateNew) : high_(0), low_(0), stringUUID_(""), isValid_(false) {
        if (generateNew) InitUUID();
    }
    /// @brief コンストラクタ（既存のUUID文字列から生成）
    explicit UUID128(const std::string &uuidStr) : high_(0), low_(0), stringUUID_(""), isValid_(false) {
        StringToUUID(uuidStr);
    }
    /// @brief コンストラクタ（既存の上位64ビットと下位64ビットから生成）
    explicit UUID128(uint64_t high, uint64_t low) : high_(high), low_(low), stringUUID_(""), isValid_(false) {
        UUIDToString();
    }
    ~UUID128() = default;

    bool operator==(const UUID128 &other) const { return high_ == other.high_ && low_ == other.low_; }
    bool operator!=(const UUID128 &other) const { return !(*this == other); }

    /// @brief UUIDを生成する（新しいUUIDを生成して設定する）
    void GenerateUUID() { InitUUID(); }
    /// @brief UUIDをリセットする（無効化する）
    void ResetUUID() { ClearUUID(); }
    /// @brief UUIDの文字列を取得する
    const std::string &ToString() const { return stringUUID_; }
    /// @brief UUIDの上位64ビットを取得する
    uint64_t GetHigh() const { return high_; }
    /// @brief UUIDの下位64ビットを取得する
    uint64_t GetLow() const { return low_; }
    /// @brief UUIDが有効かどうかを取得する（UUIDが生成されているかどうか）
    bool IsValid() const { return isValid_; }
};

} // namespace KashipanEngine

// std::hash の特殊化を提供して、UUID128 を unordered_map などで使用できるようにする
namespace std {
template<>
struct hash<KashipanEngine::UUID128> {
    std::size_t operator()(const KashipanEngine::UUID128 &uuid) const noexcept {
        return std::hash<std::string>()(uuid.ToString());
    }
};
} // namespace std
