#pragma once
#include <stdexcept>
#include <cmath>
#include <vector>

#include "Math/Matrix3x3.h"

struct Vector3;

struct Vector2 final {
    static const Vector2 &Zero() noexcept {
        static Vector2 zero(0.0f, 0.0f);
        return zero;
    }
    static Vector2 Lerp(const Vector2 &start, const Vector2 &end, float t) noexcept;
    static Vector2 Slerp(const Vector2 &start, const Vector2 &end, float t) noexcept;
    static Vector2 Bezier(const Vector2 &p0, const Vector2 &p1, const Vector2 &p2, float t) noexcept;
    static Vector2 CatmullRomInterpolation(const Vector2 &p0, const Vector2 &p1, const Vector2 &p2, const Vector2 &p3, float t) noexcept;
    static Vector2 CatmullRomPosition(const std::vector<Vector2> &points, float t, bool isLoop = false);

    Vector2() noexcept = default;
    constexpr Vector2(float x, float y) noexcept : x(x), y(y) {}
    explicit constexpr Vector2(float v) noexcept : x(v), y(v) {}
    Vector2(const Vector2 &vector) : x(vector.x), y(vector.y) {}
    Vector2(const Vector3 &vector);

    Vector2 &operator=(const Vector2 &vector);
    Vector2 &operator+=(const Vector2 &vector) noexcept;
    Vector2 &operator-=(const Vector2 &vector) noexcept;
    Vector2 &operator*=(float scalar) noexcept;
    Vector2 &operator*=(const Vector2 &vector) noexcept;
    Vector2 &operator/=(float scalar);
    Vector2 &operator/=(const Vector2 &vector);
    bool operator==(const Vector2 &vector) const noexcept;
    bool operator!=(const Vector2 &vector) const noexcept;

    [[nodiscard]] constexpr float Dot(const Vector2 &vector) const noexcept;
    [[nodiscard]] constexpr float Cross(const Vector2 &vector) const noexcept;
    [[nodiscard]] float Length() const noexcept;
    [[nodiscard]] constexpr float LengthSquared() const noexcept;
    [[nodiscard]] Vector2 Normalize() const;
    [[nodiscard]] Vector2 Projection(const Vector2 &vector) const noexcept;
    [[nodiscard]] Vector2 Perpendicular() const noexcept;
    [[nodiscard]] Vector2 Rejection(const Vector2 &vector) const noexcept;
    [[nodiscard]] Vector2 Refrection(const Vector2 &normal) const noexcept;
    [[nodiscard]] float Distance(const Vector2 &vector) const noexcept;

    float x;
    float y;
};

// constexpr関数は使用する全ての翻訳単位で定義が見えている必要があるため、ヘッダ内で定義する
inline constexpr float Vector2::Dot(const Vector2 &vector) const noexcept {
    return x * vector.x + y * vector.y;
}

inline constexpr float Vector2::Cross(const Vector2 &vector) const noexcept {
    return x * vector.y - y * vector.x;
}

inline constexpr float Vector2::LengthSquared() const noexcept {
    return x * x + y * y;
}

inline constexpr Vector2 operator-(const Vector2 &vector) noexcept {
    return Vector2(-vector.x, -vector.y);
}

inline constexpr Vector2 operator+(const Vector2 &a, const Vector2 &b) noexcept {
    return Vector2(a.x + b.x, a.y + b.y);
}

inline constexpr Vector2 operator-(const Vector2 &a, const Vector2 &b) noexcept {
    return Vector2(a.x - b.x, a.y - b.y);
}

inline constexpr Vector2 operator*(const Vector2 &vector, float scalar) noexcept {
    return Vector2(vector.x * scalar, vector.y * scalar);
}

inline constexpr Vector2 operator*(float scalar, const Vector2 &vector) noexcept {
    return Vector2(vector.x * scalar, vector.y * scalar);
}

inline constexpr Vector2 operator/(const Vector2 &vector, float scalar) {
    return vector * (1.0f / scalar);
}

inline constexpr Vector2 operator/(const Vector2 &a, const Vector2 &b) {
    return Vector2(a.x / b.x, a.y / b.y);
}

inline constexpr Vector2 operator*(const Matrix3x3 &matrix, const Vector2 &vector) noexcept {
    return Vector2(
        matrix.m[0][0] * vector.x + matrix.m[1][0] * vector.y + matrix.m[2][0],
        matrix.m[0][1] * vector.x + matrix.m[1][1] * vector.y + matrix.m[2][1]
    );
}

inline constexpr Vector2 operator*(const Vector2 &vector, const Matrix3x3 &matrix) noexcept {
    return Vector2(
        vector.x * matrix.m[0][0] + vector.y * matrix.m[0][1] + matrix.m[0][2],
        vector.x * matrix.m[1][0] + vector.y * matrix.m[1][1] + matrix.m[1][2]
    );
}