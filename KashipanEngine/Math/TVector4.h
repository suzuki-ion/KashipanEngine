#pragma once
#include <type_traits>

namespace KashipanEngine {

template<typename T>
struct TVector2;
template<typename T>
struct TVector3;

template<typename T>
struct TVector4 final {
    static_assert(std::is_arithmetic_v<T>, "T must be an arithmetic type");

    static TVector4<T> Lerp(const TVector4<T> &start, const TVector4<T> &end, T t) noexcept;

    TVector4() noexcept = default;
    constexpr TVector4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
    explicit constexpr TVector4(T value) : x(value), y(value), z(value), w(value) {}
    TVector4(const TVector2<T> &vector2) noexcept;
    TVector4(const TVector3<T> &vector3) noexcept;

    TVector4<T> &operator=(const TVector4<T> &vector) noexcept;
    TVector4<T> &operator+=(const TVector4<T> &vector) noexcept;
    TVector4<T> &operator-=(const TVector4<T> &vector) noexcept;
    TVector4<T> &operator*=(T scalar) noexcept;
    TVector4<T> &operator*=(const TVector4<T> &vector) noexcept;
    TVector4<T> &operator/=(T scalar);
    TVector4<T> &operator/=(const TVector4<T> &vector);
    bool operator==(const TVector4<T> &vector) const noexcept;
    bool operator!=(const TVector4<T> &vector) const noexcept;

    [[nodiscard]] constexpr T Dot(const TVector4<T> &vector) const noexcept;
    [[nodiscard]] T Length() const noexcept;
    [[nodiscard]] constexpr T LengthSquared() const noexcept;
    [[nodiscard]] TVector4<T> Normalize() const;

    T x;
    T y;
    T z;
    T w;
};

template<typename T>
inline constexpr TVector4<T> operator-(const TVector4<T> &vector) noexcept {
    return TVector4<T>(-vector.x, -vector.y, -vector.z, -vector.w);
}

template<typename T>
inline constexpr TVector4<T> operator+(const TVector4<T> &a, const TVector4<T> &b) noexcept {
    return TVector4<T>(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

template<typename T>
inline constexpr TVector4<T> operator-(const TVector4<T> &a, const TVector4<T> &b) noexcept {
    return TVector4<T>(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

template<typename T>
inline constexpr TVector4<T> operator*(const TVector4<T> &vector, T scalar) noexcept {
    return TVector4<T>(vector.x * scalar, vector.y * scalar, vector.z * scalar, vector.w * scalar);
}

template<typename T>
inline constexpr TVector4<T> operator*(T scalar, const TVector4<T> &vector) noexcept {
    return TVector4<T>(vector.x * scalar, vector.y * scalar, vector.z * scalar, vector.w * scalar);
}

template<typename T>
inline constexpr TVector4<T> operator/(const TVector4<T> &vector, T scalar) {
    if constexpr (std::is_integral_v<T>) {
        return TVector4<T>(vector.x / scalar, vector.y / scalar, vector.z / scalar, vector.w / scalar);
    } else {
        return vector * (T(1) / scalar);
    }
}

template<typename T>
inline constexpr TVector4<T> operator/(const TVector4<T> &a, const TVector4<T> &b) {
    return TVector4<T>(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w);
}

// 型エイリアス
using Vector4i = TVector4<int>;
using Vector4f = TVector4<float>;
using Vector4d = TVector4<double>;

} // namespace KashipanEngine