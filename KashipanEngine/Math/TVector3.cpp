#include "TVector3.h"
#include "TVector2.h"
#include "TVector4.h"
#include "TMatrix4x4.h"
#include <cassert>
#include <algorithm>
#include <cmath>

namespace KashipanEngine {

// TMathUtilsの関数を直接実装する（循環依存を避けるため）
namespace TMathUtils {
    template<typename T>
    TVector3<T> Lerp(const TVector3<T> &start, const TVector3<T> &end, T t) noexcept {
        return TVector3<T>(
            start.x + t * (end.x - start.x),
            start.y + t * (end.y - start.y),
            start.z + t * (end.z - start.z)
        );
    }

    template<typename T>
    TVector3<T> Slerp(const TVector3<T> &start, const TVector3<T> &end, T t) noexcept {
        T dot = start.x * end.x + start.y * end.y + start.z * end.z;
        
        // ベクトルが非常に近い場合は線形補間を使用
        if constexpr (std::is_floating_point_v<T>) {
            if (std::abs(dot) > T(0.9995)) {
                return Lerp(start, end, t);
            }
        }
        
        T theta = std::acos(std::clamp(dot, T(-1), T(1)));
        T sinTheta = std::sin(theta);
        
        if (sinTheta == T(0)) {
            return start;
        }
        
        T ratioA = std::sin((T(1) - t) * theta) / sinTheta;
        T ratioB = std::sin(t * theta) / sinTheta;
        
        return TVector3<T>(
            ratioA * start.x + ratioB * end.x,
            ratioA * start.y + ratioB * end.y,
            ratioA * start.z + ratioB * end.z
        );
    }

    template<typename T>
    TVector3<T> Bezier(const TVector3<T> &p0, const TVector3<T> &p1, const TVector3<T> &p2, T t) noexcept {
        T u = T(1) - t;
        T tt = t * t;
        T uu = u * u;
        
        return TVector3<T>(
            uu * p0.x + T(2) * u * t * p1.x + tt * p2.x,
            uu * p0.y + T(2) * u * t * p1.y + tt * p2.y,
            uu * p0.z + T(2) * u * t * p1.z + tt * p2.z
        );
    }

    template<typename T>
    TVector3<T> CatmullRomInterpolation(const TVector3<T> &p0, const TVector3<T> &p1, const TVector3<T> &p2, const TVector3<T> &p3, T t) noexcept {
        T tt = t * t;
        T ttt = tt * t;
        
        T q1 = -ttt + T(2) * tt - t;
        T q2 = T(3) * ttt - T(5) * tt + T(2);
        T q3 = -T(3) * ttt + T(4) * tt + t;
        T q4 = ttt - tt;
        
        return TVector3<T>(
            T(0.5) * (p0.x * q1 + p1.x * q2 + p2.x * q3 + p3.x * q4),
            T(0.5) * (p0.y * q1 + p1.y * q2 + p2.y * q3 + p3.y * q4),
            T(0.5) * (p0.z * q1 + p1.z * q2 + p2.z * q3 + p3.z * q4)
        );
    }

    template<typename T>
    TVector3<T> CatmullRomPosition(const std::vector<TVector3<T>> &points, T t, bool isLoop) {
        if (points.size() < 2) {
            return TVector3<T>();
        }
        
        if (points.size() == 2) {
            return Lerp(points[0], points[1], t);
        }
        
        size_t numSegments = isLoop ? points.size() : points.size() - 1;
        T segmentT = t * T(numSegments);
        size_t segmentIndex = static_cast<size_t>(segmentT);
        T localT = segmentT - T(segmentIndex);
        
        if (segmentIndex >= numSegments) {
            segmentIndex = numSegments - 1;
            localT = T(1);
        }
        
        auto getPoint = [&](int index) -> TVector3<T> {
            if (isLoop) {
                while (index < 0) index += static_cast<int>(points.size());
                while (index >= static_cast<int>(points.size())) index -= static_cast<int>(points.size());
            } else {
                index = std::clamp(index, 0, static_cast<int>(points.size()) - 1);
            }
            return points[index];
        };
        
        TVector3<T> p0 = getPoint(static_cast<int>(segmentIndex) - 1);
        TVector3<T> p1 = getPoint(static_cast<int>(segmentIndex));
        TVector3<T> p2 = getPoint(static_cast<int>(segmentIndex) + 1);
        TVector3<T> p3 = getPoint(static_cast<int>(segmentIndex) + 2);
        
        return CatmullRomInterpolation(p0, p1, p2, p3, localT);
    }

    template<typename T>
    constexpr T Dot(const TVector3<T> &vector1, const TVector3<T> &vector2) noexcept {
        return vector1.x * vector2.x + vector1.y * vector2.y + vector1.z * vector2.z;
    }

    template<typename T>
    TVector3<T> Cross(const TVector3<T> &vector1, const TVector3<T> &vector2) noexcept {
        return TVector3<T>(
            vector1.y * vector2.z - vector1.z * vector2.y,
            vector1.z * vector2.x - vector1.x * vector2.z,
            vector1.x * vector2.y - vector1.y * vector2.x
        );
    }

    template<typename T>
    T Length(const TVector3<T> &vector) noexcept {
        if constexpr (std::is_floating_point_v<T>) {
            return std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
        } else {
            return static_cast<T>(std::sqrt(static_cast<double>(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z)));
        }
    }

    template<typename T>
    constexpr T LengthSquared(const TVector3<T> &vector) noexcept {
        return vector.x * vector.x + vector.y * vector.y + vector.z * vector.z;
    }

    template<typename T>
    TVector3<T> Normalize(const TVector3<T> &vector) {
        T length = Length(vector);
        if (length == T(0)) {
            return TVector3<T>(T(0), T(0), T(0));
        }
        return TVector3<T>(vector.x / length, vector.y / length, vector.z / length);
    }

    template<typename T>
    TVector3<T> Projection(const TVector3<T> &vector, const TVector3<T> &onto) noexcept {
        T ontoLengthSq = LengthSquared(onto);
        if (ontoLengthSq == T(0)) {
            return TVector3<T>(T(0), T(0), T(0));
        }
        T projectionLength = Dot(vector, onto) / ontoLengthSq;
        return TVector3<T>(onto.x * projectionLength, onto.y * projectionLength, onto.z * projectionLength);
    }

    template<typename T>
    TVector3<T> Perpendicular(const TVector3<T> &vector) noexcept {
        // 適当な軸と外積を取って垂直ベクトルを作成
        TVector3<T> axis = (std::abs(vector.x) < T(0.9)) ? TVector3<T>(T(1), T(0), T(0)) : TVector3<T>(T(0), T(1), T(0));
        return Cross(vector, axis);
    }

    template<typename T>
    TVector3<T> Rejection(const TVector3<T> &vector, const TVector3<T> &onto) noexcept {
        return vector - Projection(vector, onto);
    }

    template<typename T>
    TVector3<T> Reflection(const TVector3<T> &vector, const TVector3<T> &normal) noexcept {
        return vector - T(2) * Dot(vector, normal) * normal;
    }

    template<typename T>
    T Distance(const TVector3<T> &vector1, const TVector3<T> &vector2) noexcept {
        return Length(vector2 - vector1);
    }
}

template<typename T>
TVector3<T> TVector3<T>::Lerp(const TVector3<T> &start, const TVector3<T> &end, T t) noexcept {
    return TMathUtils::Lerp(start, end, t);
}

template<typename T>
TVector3<T> TVector3<T>::Slerp(const TVector3<T> &start, const TVector3<T> &end, T t) noexcept {
    return TMathUtils::Slerp(start, end, t);
}

template<typename T>
TVector3<T> TVector3<T>::Bezier(const TVector3<T> &p0, const TVector3<T> &p1, const TVector3<T> &p2, const T t) noexcept {
    return TMathUtils::Bezier(p0, p1, p2, t);
}

template<typename T>
TVector3<T> TVector3<T>::CatmullRomInterpolation(const TVector3<T> &p0, const TVector3<T> &p1, const TVector3<T> &p2, const TVector3<T> &p3, T t) noexcept {
    return TMathUtils::CatmullRomInterpolation(p0, p1, p2, p3, t);
}

template<typename T>
TVector3<T> TVector3<T>::CatmullRomPosition(const std::vector<TVector3<T>> &points, T t, bool isLoop) {
    return TMathUtils::CatmullRomPosition(points, t, isLoop);
}

template<typename T>
TVector3<T>::TVector3(const TVector2<T> &vector) noexcept {
    x = vector.x;
    y = vector.y;
    z = T(0);
}

template<typename T>
TVector3<T>::TVector3(const TVector4<T> &vector) noexcept {
    x = vector.x;
    y = vector.y;
    z = vector.z;
}

template<typename T>
TVector3<T> &TVector3<T>::operator=(const TVector3<T> &vector) noexcept {
    x = vector.x;
    y = vector.y;
    z = vector.z;
    return *this;
}

template<typename T>
TVector3<T> &TVector3<T>::operator+=(const TVector3<T> &vector) noexcept {
    x += vector.x;
    y += vector.y;
    z += vector.z;
    return *this;
}

template<typename T>
TVector3<T> &TVector3<T>::operator-=(const TVector3<T> &vector) noexcept {
    x -= vector.x;
    y -= vector.y;
    z -= vector.z;
    return *this;
}

template<typename T>
TVector3<T> &TVector3<T>::operator*=(const T scalar) noexcept {
    x *= scalar;
    y *= scalar;
    z *= scalar;
    return *this;
}

template<typename T>
TVector3<T> &TVector3<T>::operator*=(const TVector3<T> &vector) noexcept {
    x *= vector.x;
    y *= vector.y;
    z *= vector.z;
    return *this;
}

template<typename T>
TVector3<T> &TVector3<T>::operator/=(const T scalar) {
    if constexpr (std::is_integral_v<T>) {
        x /= scalar;
        y /= scalar;
        z /= scalar;
    } else {
        if (scalar == T(0)) {
            x = T(0);
            y = T(0);
            z = T(0);
        } else {
            T inv = T(1) / scalar;
            x *= inv;
            y *= inv;
            z *= inv;
        }
    }
    return *this;
}

template<typename T>
TVector3<T> &TVector3<T>::operator/=(const TVector3<T> &vector) {
    if constexpr (std::is_integral_v<T>) {
        x /= vector.x;
        y /= vector.y;
        z /= vector.z;
    } else {
        x = (vector.x != T(0)) ? x / vector.x : T(0);
        y = (vector.y != T(0)) ? y / vector.y : T(0);
        z = (vector.z != T(0)) ? z / vector.z : T(0);
    }
    return *this;
}

template<typename T>
bool TVector3<T>::operator==(const TVector3<T> &vector) const noexcept {
    return x == vector.x && y == vector.y && z == vector.z;
}

template<typename T>
bool TVector3<T>::operator!=(const TVector3<T> &vector) const noexcept {
    return x != vector.x || y != vector.y || z != vector.z;
}

template<typename T>
constexpr T TVector3<T>::Dot(const TVector3<T> &vector) const noexcept {
    return TMathUtils::Dot(*this, vector);
}

template<typename T>
TVector3<T> TVector3<T>::Cross(const TVector3<T> &vector) const noexcept {
    return TMathUtils::Cross(*this, vector);
}

template<typename T>
T TVector3<T>::Length() const noexcept {
    return TMathUtils::Length(*this);
}

template<typename T>
constexpr T TVector3<T>::LengthSquared() const noexcept {
    return TMathUtils::LengthSquared(*this);
}

template<typename T>
TVector3<T> TVector3<T>::Normalize() const {
    return TMathUtils::Normalize(*this);
}

template<typename T>
TVector3<T> TVector3<T>::Projection(const TVector3<T> &vector) const noexcept {
    return TMathUtils::Projection(*this, vector);
}

template<typename T>
TVector3<T> TVector3<T>::Perpendicular() const noexcept {
    return TMathUtils::Perpendicular(*this);
}

template<typename T>
TVector3<T> TVector3<T>::Rejection(const TVector3<T> &vector) const noexcept {
    return TMathUtils::Rejection(*this, vector);
}

template<typename T>
TVector3<T> TVector3<T>::Refrection(const TVector3<T> &normal) const noexcept {
    return TMathUtils::Reflection(*this, normal);
}

template<typename T>
T TVector3<T>::Distance(const TVector3<T> &vector) const {
    return TMathUtils::Distance(*this, vector);
}

template<typename T>
TVector3<T> TVector3<T>::ClosestPoint(const Math::TSegment<T> &segment) const noexcept {
    // 一時的にダミー実装（後でTSegmentを実装したら修正）
    return *this;
}

template<typename T>
TVector3<T> TVector3<T>::Transform(const TMatrix4x4<T> &mat) const noexcept {
    return TVector3<T>(
        mat.m[0][0] * x + mat.m[1][0] * y + mat.m[2][0] * z + mat.m[3][0],
        mat.m[0][1] * x + mat.m[1][1] * y + mat.m[2][1] * z + mat.m[3][1],
        mat.m[0][2] * x + mat.m[1][2] * y + mat.m[2][2] * z + mat.m[3][2]
    );
}

template<typename T>
const TVector3<T> operator*(const TMatrix4x4<T> &mat, const TVector3<T> &vector) noexcept {
    return vector.Transform(mat);
}

template<typename T>
const TVector3<T> operator*(const TVector3<T> &vector, const TMatrix4x4<T> &mat) noexcept {
    return TVector3<T>(
        vector.x * mat.m[0][0] + vector.y * mat.m[0][1] + vector.z * mat.m[0][2] + mat.m[0][3],
        vector.x * mat.m[1][0] + vector.y * mat.m[1][1] + vector.z * mat.m[1][2] + mat.m[1][3],
        vector.x * mat.m[2][0] + vector.y * mat.m[2][1] + vector.z * mat.m[2][2] + mat.m[2][3]
    );
}

// 明示的インスタンス化
template struct TVector3<int>;
template struct TVector3<float>;
template struct TVector3<double>;

} // namespace KashipanEngine