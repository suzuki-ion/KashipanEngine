#include "TVector4.h"
#include "TVector2.h"
#include "TVector3.h"
#include <cassert>
#include <cmath>

namespace KashipanEngine {

// TMathUtilsの関数を直接実装する（循環依存を避けるため）
namespace TMathUtils {
    template<typename T>
    TVector4<T> Lerp(const TVector4<T> &start, const TVector4<T> &end, T t) noexcept {
        return TVector4<T>(
            start.x + t * (end.x - start.x),
            start.y + t * (end.y - start.y),
            start.z + t * (end.z - start.z),
            start.w + t * (end.w - start.w)
        );
    }

    template<typename T>
    constexpr T Dot(const TVector4<T> &vector1, const TVector4<T> &vector2) noexcept {
        return vector1.x * vector2.x + vector1.y * vector2.y + vector1.z * vector2.z + vector1.w * vector2.w;
    }

    template<typename T>
    T Length(const TVector4<T> &vector) noexcept {
        if constexpr (std::is_floating_point_v<T>) {
            return std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z + vector.w * vector.w);
        } else {
            return static_cast<T>(std::sqrt(static_cast<double>(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z + vector.w * vector.w)));
        }
    }

    template<typename T>
    constexpr T LengthSquared(const TVector4<T> &vector) noexcept {
        return vector.x * vector.x + vector.y * vector.y + vector.z * vector.z + vector.w * vector.w;
    }

    template<typename T>
    TVector4<T> Normalize(const TVector4<T> &vector) {
        T length = Length(vector);
        if (length == T(0)) {
            return TVector4<T>(T(0), T(0), T(0), T(0));
        }
        return TVector4<T>(vector.x / length, vector.y / length, vector.z / length, vector.w / length);
    }
}

template<typename T>
TVector4<T> TVector4<T>::Lerp(const TVector4<T> &start, const TVector4<T> &end, T t) noexcept {
    return TMathUtils::Lerp(start, end, t);
}

template<typename T>
TVector4<T>::TVector4(const TVector2<T> &vector2) noexcept {
    x = vector2.x;
    y = vector2.y;
    z = T(0);
    w = T(0);
}

template<typename T>
TVector4<T>::TVector4(const TVector3<T> &vector3) noexcept {
    x = vector3.x;
    y = vector3.y;
    z = vector3.z;
    w = T(0);
}

template<typename T>
TVector4<T> &TVector4<T>::operator=(const TVector4<T> &vector) noexcept {
    x = vector.x;
    y = vector.y;
    z = vector.z;
    w = vector.w;
    return *this;
}

template<typename T>
TVector4<T> &TVector4<T>::operator+=(const TVector4<T> &vector) noexcept {
    x += vector.x;
    y += vector.y;
    z += vector.z;
    w += vector.w;
    return *this;
}

template<typename T>
TVector4<T> &TVector4<T>::operator-=(const TVector4<T> &vector) noexcept {
    x -= vector.x;
    y -= vector.y;
    z -= vector.z;
    w -= vector.w;
    return *this;
}

template<typename T>
TVector4<T> &TVector4<T>::operator*=(T scalar) noexcept {
    x *= scalar;
    y *= scalar;
    z *= scalar;
    w *= scalar;
    return *this;
}

template<typename T>
TVector4<T> &TVector4<T>::operator*=(const TVector4<T> &vector) noexcept {
    x *= vector.x;
    y *= vector.y;
    z *= vector.z;
    w *= vector.w;
    return *this;
}

template<typename T>
TVector4<T> &TVector4<T>::operator/=(T scalar) {
    if constexpr (std::is_integral_v<T>) {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        w /= scalar;
    } else {
        if (scalar == T(0)) {
            x = T(0);
            y = T(0);
            z = T(0);
            w = T(0);
        } else {
            T inv = T(1) / scalar;
            x *= inv;
            y *= inv;
            z *= inv;
            w *= inv;
        }
    }
    return *this;
}

template<typename T>
TVector4<T> &TVector4<T>::operator/=(const TVector4<T> &vector) {
    if constexpr (std::is_integral_v<T>) {
        x /= vector.x;
        y /= vector.y;
        z /= vector.z;
        w /= vector.w;
    } else {
        x = (vector.x != T(0)) ? x / vector.x : T(0);
        y = (vector.y != T(0)) ? y / vector.y : T(0);
        z = (vector.z != T(0)) ? z / vector.z : T(0);
        w = (vector.w != T(0)) ? w / vector.w : T(0);
    }
    return *this;
}

template<typename T>
bool TVector4<T>::operator==(const TVector4<T> &vector) const noexcept {
    return x == vector.x && y == vector.y && z == vector.z && w == vector.w;
}

template<typename T>
bool TVector4<T>::operator!=(const TVector4<T> &vector) const noexcept {
    return x != vector.x || y != vector.y || z != vector.z || w != vector.w;
}

template<typename T>
constexpr T TVector4<T>::Dot(const TVector4<T> &vector) const noexcept {
    return TMathUtils::Dot(*this, vector);
}

template<typename T>
T TVector4<T>::Length() const noexcept {
    return TMathUtils::Length(*this);
}

template<typename T>
constexpr T TVector4<T>::LengthSquared() const noexcept {
    return TMathUtils::LengthSquared(*this);
}

template<typename T>
TVector4<T> TVector4<T>::Normalize() const {
    return TMathUtils::Normalize(*this);
}

// 明示的インスタンス化
template struct TVector4<int>;
template struct TVector4<float>;
template struct TVector4<double>;

} // namespace KashipanEngine