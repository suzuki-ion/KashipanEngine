#pragma once
#include "Math/Vector4.h"

/// @brief 色を表す型（内部データはVector4と同じ4 float、r,g,b,aの並び）。
/// @details マテリアルの追加パラメータ編集UIでVector4と区別し、ImGui::ColorEdit4で編集するために
///          ValueTypeSystem上の別型として用意する。Vector4はfinal宣言のため継承では作れない
struct Color final {
    static const Color &White() noexcept {
        static Color white(1.0f, 1.0f, 1.0f, 1.0f);
        return white;
    }

    Color() noexcept = default;
    constexpr Color(float r, float g, float b, float a) noexcept : r(r), g(g), b(b), a(a) {}
    constexpr Color(const Vector4 &vector) noexcept : r(vector.x), g(vector.y), b(vector.z), a(vector.w) {}

    constexpr operator Vector4() const noexcept { return Vector4(r, g, b, a); }

    bool operator==(const Color &color) const noexcept { return r == color.r && g == color.g && b == color.b && a == color.a; }
    bool operator!=(const Color &color) const noexcept { return !(*this == color); }

    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};
