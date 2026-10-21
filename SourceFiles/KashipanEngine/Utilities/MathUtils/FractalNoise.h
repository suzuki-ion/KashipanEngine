#pragma once

#include "PerlinNoise.h"

namespace KashipanEngine {

class FractalNoise {
public:
    FractalNoise(uint32_t seed = 0);

    /// @brief フラクタルノイズの値を取得する（1次元）
    /// @param x 座標x
    /// @param octaves オクターブ数（デフォルトは4）
    /// @param lacunarity 各オクターブの周波数倍率（デフォルトは2.0f）
    /// @param persistence 各オクターブの振幅倍率（デフォルトは0.5f）
    /// @return フラクタルノイズの値（-1.0〜1.0）
    float GetValue(float x, int octaves = 4, float lacunarity = 2.0f, float persistence = 0.5f) const;

    /// @brief フラクタルノイズの値を取得する（2次元）
    /// @param x 座標x
    /// @param y 座標y
    /// @param octaves オクターブ数（デフォルトは4）
    /// @param lacunarity 各オクターブの周波数倍率（デフォルトは2.0f）
    /// @param persistence 各オクターブの振幅倍率（デフォルトは0.5f）
    /// @return フラクタルノイズの値（-1.0〜1.0）
    float GetValue(float x, float y, int octaves = 4, float lacunarity = 2.0f, float persistence = 0.5f) const;

    /// @brief フラクタルノイズの値を取得する（3次元）
    /// @param x 座標x
    /// @param y 座標y
    /// @param z 座標z
    /// @param octaves オクターブ数（デフォルトは4）
    /// @param lacunarity 各オクターブの周波数倍率（デフォルトは2.0f）
    /// @param persistence 各オクターブの振幅倍率（デフォルトは0.5f）
    /// @return フラクタルノイズの値（-1.0〜1.0）
    float GetValue(float x, float y, float z, int octaves = 4, float lacunarity = 2.0f, float persistence = 0.5f) const;
    
private:
    PerlinNoise pn_; 
};

} // namespace KashipanEngine
