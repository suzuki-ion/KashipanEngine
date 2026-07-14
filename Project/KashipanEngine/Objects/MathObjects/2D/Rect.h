#pragma once

#include "Math/Vector2.h"

namespace KashipanEngine {
namespace Math {

struct Rect final {
    Vector2 center{0.0f, 0.0f};
    Vector2 halfSize{0.0f, 0.0f};
    /// @brief Z軸回転（ラジアン）。0以外の場合、当たり判定・デバッグ描画はOBBとして扱われる
    float rotation = 0.0f;
};

} // namespace Math
} // namespace KashipanEngine
