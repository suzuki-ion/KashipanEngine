#pragma once
#include <random>

namespace KashipanEngine {

/// @brief min以上max以下のランダムな値を取得する
/// @tparam T 数値型（int、float、doubleなど）
/// @param min 最低値
/// @param max 最高値
/// @return ランダムな値
template<typename T>
T GetRandomValue(T min, T max) {
    static std::random_device rd;
    static std::mt19937 mtEngine(rd());
    if constexpr (std::is_integral_v<T>) {
        std::uniform_int_distribution<T> dist(min, max);
        return dist(mtEngine);
    } else if constexpr (std::is_floating_point_v<T>) {
        std::uniform_real_distribution<T> dist(min, max);
        return dist(mtEngine);
    } else {
        static_assert(std::is_arithmetic_v<T>, "T must be an arithmetic type");
        return min; // 非数値型の場合はminを返す（実際にはこの関数は使用できない）
    }
}

/// @brief min以上max以下のランダムな整数を取得する
/// @param min 最低値
/// @param max 最高値
/// @return ランダムな整数
int GetRandomInt(int min, int max);

/// @brief min以上max以下のランダムな浮動小数点数を取得する
/// @param min 最低値
/// @param max 最高値
/// @return ランダムな浮動小数点数
float GetRandomFloat(float min, float max);

/// @brief min以上max以下のランダムな倍精度浮動小数点数を取得する
/// @param min 最低値
/// @param max 最高値
/// @return ランダムな倍精度浮動小数点数
double GetRandomDouble(double min, double max);

/// @brief min以上max以下のランダムな64ビット整数を取得する
/// @param min 最低値
/// @param max 最高値
/// @return ランダムな64ビット整数
int64_t GetRandomInt64(int64_t min, int64_t max);

/// @brief ランダムな真偽値を取得する
/// @param trueProbability trueを返す確率（0.0〜1.0）
/// @return ランダムな真偽値
bool GetRandomBool(float trueProbability = 0.5f);

} // namespace KashipanEngine