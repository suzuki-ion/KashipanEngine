#pragma once
#include <stdexcept>
#include <cmath>
#include <vector>
#include <type_traits>

namespace KashipanEngine {

template<typename T>
struct TVector3;
template<typename T>
struct TMatrix3x3;

namespace Math {
template<typename T>
struct TSegment;
} // namespace Math

template<typename T>
struct TVector2 final {
    static_assert(std::is_arithmetic_v<T>, "T must be an arithmetic type");

    static TVector2<T> Lerp(const TVector2<T> &start, const TVector2<T> &end, T t) noexcept;
    static TVector2<T> Slerp(const TVector2<T> &start, const TVector2<T> &end, T t) noexcept;
    static TVector2<T> Bezier(const TVector2<T> &p0, const TVector2<T> &p1, const TVector2<T> &p2, T t) noexcept;
    static TVector2<T> CatmullRomInterpolation(const TVector2<T> &p0, const TVector2<T> &p1, const TVector2<T> &p2, const TVector2<T> &p3, T t) noexcept;
    static TVector2<T> CatmullRomPosition(const std::vector<TVector2<T>> &points, T t, bool isLoop = false);

    TVector2() noexcept = default;
    constexpr TVector2(T x, T y) noexcept : x(x), y(y) {}
    explicit constexpr TVector2(T v) noexcept : x(v), y(v) {}
    TVector2(const TVector2<T> &vector) : x(vector.x), y(vector.y) {}
    TVector2(const TVector3<T> &vector);

    TVector2<T> &operator=(const TVector2<T> &vector);
    TVector2<T> &operator+=(const TVector2<T> &vector) noexcept;
    TVector2<T> &operator-=(const TVector2<T> &vector) noexcept;
    TVector2<T> &operator*=(T scalar) noexcept;
    TVector2<T> &operator*=(const TVector2<T> &vector) noexcept;
    TVector2<T> &operator/=(T scalar);
    TVector2<T> &operator/=(const TVector2<T> &vector);
    bool operator==(const TVector2<T> &vector) const noexcept;
    bool operator!=(const TVector2<T> &vector) const noexcept;

    [[nodiscard]] constexpr T Dot(const TVector2<T> &vector) const noexcept;
    [[nodiscard]] constexpr T Cross(const TVector2<T> &vector) const noexcept;
    [[nodiscard]] T Length() const noexcept;
    [[nodiscard]] constexpr T LengthSquared() const noexcept;
    [[nodiscard]] TVector2<T> Normalize() const;
    [[nodiscard]] TVector2<T> Projection(const TVector2<T> &vector) const noexcept;
    [[nodiscard]] TVector2<T> ClosestPoint(const Math::TSegment<T> &segment) const noexcept;
    [[nodiscard]] TVector2<T> Perpendicular() const noexcept;
    [[nodiscard]] TVector2<T> Rejection(const TVector2<T> &vector) const noexcept;
    [[nodiscard]] TVector2<T> Refrection(const TVector2<T> &normal) const noexcept;
    [[nodiscard]] T Distance(const TVector2<T> &vector) const noexcept;

    T x;
    T y;
};

template<typename T>
inline constexpr TVector2<T> operator-(const TVector2<T> &vector) noexcept {
    return TVector2<T>(-vector.x, -vector.y);
}

template<typename T>
inline constexpr TVector2<T> operator+(const TVector2<T> &a, const TVector2<T> &b) noexcept {
    return TVector2<T>(a.x + b.x, a.y + b.y);
}

template<typename T>
inline constexpr TVector2<T> operator-(const TVector2<T> &a, const TVector2<T> &b) noexcept {
    return TVector2<T>(a.x - b.x, a.y - b.y);
}

template<typename T>
inline constexpr TVector2<T> operator*(const TVector2<T> &vector, T scalar) noexcept {
    return TVector2<T>(vector.x * scalar, vector.y * scalar);
}

template<typename T>
inline constexpr TVector2<T> operator*(T scalar, const TVector2<T> &vector) noexcept {
    return TVector2<T>(vector.x * scalar, vector.y * scalar);
}

template<typename T>
inline constexpr TVector2<T> operator/(const TVector2<T> &vector, T scalar) {
    if constexpr (std::is_integral_v<T>) {
        return TVector2<T>(vector.x / scalar, vector.y / scalar);
    } else {
        return vector * (T(1) / scalar);
    }
}

template<typename T>
inline constexpr TVector2<T> operator/(const TVector2<T> &a, const TVector2<T> &b) {
    return TVector2<T>(a.x / b.x, a.y / b.y);
}

template<typename T>
inline constexpr TVector2<T> operator*(const TMatrix3x3<T> &matrix, const TVector2<T> &vector) noexcept;

template<typename T>
inline constexpr TVector2<T> operator*(const TVector2<T> &vector, const TMatrix3x3<T> &matrix) noexcept;

// 型エイリアス
using Vector2i = TVector2<int>;
using Vector2f = TVector2<float>;
using Vector2d = TVector2<double>;

} // namespace KashipanEngine