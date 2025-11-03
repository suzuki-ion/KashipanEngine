#pragma once

namespace KashipanEngine {

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

/// @brief ランダムな真偽値を取得する
/// @param trueProbability trueを返す確率（0.0〜1.0）
/// @return ランダムな真偽値
bool GetRandomBool(float trueProbability = 0.5f);

} // namespace KashipanEngine