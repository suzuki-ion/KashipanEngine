#pragma once
#include <stdexcept>
#include <cmath>
#include <vector>
#include <type_traits>

namespace KashipanEngine {

template<typename T>
struct TVector2;
template<typename T>
struct TVector4;
template<typename T>
struct TMatrix4x4;

namespace Math {
template<typename T>
struct TSegment;
} // namespace Math

template<typename T>
struct TVector3 final {
    static_assert(std::is_arithmetic_v<T>, "T must be an arithmetic type");

    static TVector3<T> Lerp(const TVector3<T> &start, const TVector3<T> &end, T t) noexcept;
    static TVector3<T> Slerp(const TVector3<T> &start, const TVector3<T> &end, T t) noexcept;
    static TVector3<T> Bezier(const TVector3<T> &p0, const TVector3<T> &p1, const TVector3<T> &p2, const T t) noexcept;
    static TVector3<T> CatmullRomInterpolation(const TVector3<T> &p0, const TVector3<T> &p1, const TVector3<T> &p2, const TVector3<T> &p3, T t) noexcept;
    static TVector3<T> CatmullRomPosition(const std::vector<TVector3<T>> &points, T t, bool isLoop = false);

    TVector3() noexcept = default;
    constexpr TVector3(T x, T y, T z) noexcept : x(x), y(y), z(z) {}
    explicit constexpr TVector3(T v) noexcept : x(v), y(v), z(v) {}
    TVector3(const TVector3<T> &vector) : x(vector.x), y(vector.y), z(vector.z) {}
    TVector3(const TVector2<T> &vector) noexcept;
    TVector3(const TVector4<T> &vector) noexcept;

    TVector3<T> &operator=(const TVector3<T> &vector) noexcept;
    TVector3<T> &operator+=(const TVector3<T> &vector) noexcept;
    TVector3<T> &operator-=(const TVector3<T> &vector) noexcept;
    TVector3<T> &operator*=(const T scalar) noexcept;
    TVector3<T> &operator*=(const TVector3<T> &vector) noexcept;
    TVector3<T> &operator/=(const T scalar);
    TVector3<T> &operator/=(const TVector3<T> &vector);
    bool operator==(const TVector3<T> &vector) const noexcept;
    bool operator!=(const TVector3<T> &vector) const noexcept;

    [[nodiscard]] constexpr T Dot(const TVector3<T> &vector) const noexcept;
    [[nodiscard]] TVector3<T> Cross(const TVector3<T> &vector) const noexcept;
    [[nodiscard]] T Length() const noexcept;
    [[nodiscard]] constexpr T LengthSquared() const noexcept;
    [[nodiscard]] TVector3<T> Normalize() const;
    [[nodiscard]] TVector3<T> Projection(const TVector3<T> &vector) const noexcept;
    [[nodiscard]] TVector3<T> ClosestPoint(const Math::TSegment<T> &segment) const noexcept;
    [[nodiscard]] TVector3<T> Perpendicular() const noexcept;
    [[nodiscard]] TVector3<T> Rejection(const TVector3<T> &vector) const noexcept;
    [[nodiscard]] TVector3<T> Refrection(const TVector3<T> &normal) const noexcept;
    [[nodiscard]] T Distance(const TVector3<T> &vector) const;
    [[nodiscard]] TVector3<T> Transform(const TMatrix4x4<T> &mat) const noexcept;

    T x;
    T y;
    T z;
};

template<typename T>
inline constexpr const TVector3<T> operator-(const TVector3<T> &vector) noexcept {
    return TVector3<T>(-vector.x, -vector.y, -vector.z);
}

template<typename T>
inline constexpr const TVector3<T> operator+(const TVector3<T> &vector1, const TVector3<T> &vector2) noexcept {
    return TVector3<T>(vector1.x + vector2.x, vector1.y + vector2.y, vector1.z + vector2.z);
}

template<typename T>
inline constexpr const TVector3<T> operator-(const TVector3<T> &vector1, const TVector3<T> &vector2) noexcept {
    return TVector3<T>(vector1.x - vector2.x, vector1.y - vector2.y, vector1.z - vector2.z);
}

template<typename T>
inline constexpr const TVector3<T> operator*(const TVector3<T> &vector, const T scalar) noexcept {
    return TVector3<T>(vector.x * scalar, vector.y * scalar, vector.z * scalar);
}

template<typename T>
inline constexpr const TVector3<T> operator*(const T scalar, const TVector3<T> &vector) noexcept {
    return TVector3<T>(scalar * vector.x, scalar * vector.y, scalar * vector.z);
}

template<typename T>
inline constexpr const TVector3<T> operator/(const TVector3<T> &vector, const T scalar) {
    if constexpr (std::is_integral_v<T>) {
        return TVector3<T>(vector.x / scalar, vector.y / scalar, vector.z / scalar);
    } else {
        return vector * (T(1) / scalar);
    }
}

template<typename T>
inline constexpr const TVector3<T> operator/(const TVector3<T> &vector1, const TVector3<T> &vector2) {
    return TVector3<T>(vector1.x / vector2.x, vector1.y / vector2.y, vector1.z / vector2.z);
}

template<typename T>
const TVector3<T> operator*(const TMatrix4x4<T> &mat, const TVector3<T> &vector) noexcept;

template<typename T>
const TVector3<T> operator*(const TVector3<T> &vector, const TMatrix4x4<T> &mat) noexcept;

// 型エイリアス
using Vector3i = TVector3<int>;
using Vector3f = TVector3<float>;
using Vector3d = TVector3<double>;

} // namespace KashipanEngine