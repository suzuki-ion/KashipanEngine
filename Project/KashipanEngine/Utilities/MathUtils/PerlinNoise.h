#pragma once

#include <vector>
#include <cstdint>

namespace KashipanEngine {

class PerlinNoise {
public:
    PerlinNoise(uint32_t seed = 0);

    /// @brief 1次元パーリンノイズの値を取得する
    /// @param x 座標x
    /// @return パーリンノイズの値（-1.0〜1.0）
    float GetValue(float x) const;

    /// @brief 2次元パーリンノイズの値を取得する
    /// @param x 座標x
    /// @param y 座標y
    /// @return パーリンノイズの値（-1.0〜1.0）
    float GetValue(float x, float y) const;

    /// @brief 3次元パーリンノイズの値を取得する
    /// @param x 座標x
    /// @param y 座標y
    /// @param z 座標z
    /// @return パーリンノイズの値（-1.0〜1.0）
    float GetValue(float x, float y, float z) const;

private:
    std::vector<int> perm_; // permutation table

    static int FastFloor(float x);
    static float Fade(float t);
    static float Lerp(float a, float b, float t);
    static float Grad(int hash, float x, float y, float z);
};

} // namespace KashipanEngine
