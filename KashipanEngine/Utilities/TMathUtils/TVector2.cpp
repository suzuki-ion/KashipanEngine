#include "TVector2.h"
#include "Math/TVector2.h"
#include <cmath>
#include <cassert>
#include <algorithm>

namespace KashipanEngine {
namespace TMathUtils {

template<typename T>
TVector2<T> Lerp(const TVector2<T> &start, const TVector2<T> &end, T t) noexcept {
    return TVector2<T>(
        start.x + t * (end.x - start.x),
        start.y + t * (end.y - start.y)
    );
}

template<typename T>
TVector2<T> Slerp(const TVector2<T> &start, const TVector2<T> &end, T t) noexcept {
    T dot = Dot(start, end);
    
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
    
    return TVector2<T>(
        ratioA * start.x + ratioB * end.x,
        ratioA * start.y + ratioB * end.y
    );
}

template<typename T>
TVector2<T> Bezier(const TVector2<T> &p0, const TVector2<T> &p1, const TVector2<T> &p2, T t) noexcept {
    T u = T(1) - t;
    T tt = t * t;
    T uu = u * u;
    
    return TVector2<T>(
        uu * p0.x + T(2) * u * t * p1.x + tt * p2.x,
        uu * p0.y + T(2) * u * t * p1.y + tt * p2.y
    );
}

template<typename T>
TVector2<T> CatmullRomInterpolation(const TVector2<T> &p0, const TVector2<T> &p1, const TVector2<T> &p2, const TVector2<T> &p3, T t) noexcept {
    T tt = t * t;
    T ttt = tt * t;
    
    T q1 = -ttt + T(2) * tt - t;
    T q2 = T(3) * ttt - T(5) * tt + T(2);
    T q3 = -T(3) * ttt + T(4) * tt + t;
    T q4 = ttt - tt;
    
    return TVector2<T>(
        T(0.5) * (p0.x * q1 + p1.x * q2 + p2.x * q3 + p3.x * q4),
        T(0.5) * (p0.y * q1 + p1.y * q2 + p2.y * q3 + p3.y * q4)
    );
}

template<typename T>
TVector2<T> CatmullRomPosition(const std::vector<TVector2<T>> &points, T t, bool isLoop) {
    if (points.size() < 2) {
        return TVector2<T>();
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
    
    auto getPoint = [&](int index) -> TVector2<T> {
        if (isLoop) {
            while (index < 0) index += static_cast<int>(points.size());
            while (index >= static_cast<int>(points.size())) index -= static_cast<int>(points.size());
        } else {
            index = std::clamp(index, 0, static_cast<int>(points.size()) - 1);
        }
        return points[index];
    };
    
    TVector2<T> p0 = getPoint(static_cast<int>(segmentIndex) - 1);
    TVector2<T> p1 = getPoint(static_cast<int>(segmentIndex));
    TVector2<T> p2 = getPoint(static_cast<int>(segmentIndex) + 1);
    TVector2<T> p3 = getPoint(static_cast<int>(segmentIndex) + 2);
    
    return CatmullRomInterpolation(p0, p1, p2, p3, localT);
}

template<typename T>
constexpr T Dot(const TVector2<T> &vector1, const TVector2<T> &vector2) noexcept {
    return vector1.x * vector2.x + vector1.y * vector2.y;
}

template<typename T>
constexpr T Cross(const TVector2<T> &vector1, const TVector2<T> &vector2) noexcept {
    return vector1.x * vector2.y - vector1.y * vector2.x;
}

template<typename T>
T Length(const TVector2<T> &vector) noexcept {
    if constexpr (std::is_floating_point_v<T>) {
        return std::sqrt(vector.x * vector.x + vector.y * vector.y);
    } else {
        return static_cast<T>(std::sqrt(static_cast<double>(vector.x * vector.x + vector.y * vector.y)));
    }
}

template<typename T>
constexpr T LengthSquared(const TVector2<T> &vector) noexcept {
    return vector.x * vector.x + vector.y * vector.y;
}

template<typename T>
TVector2<T> Normalize(const TVector2<T> &vector) {
    T length = Length(vector);
    if (length == T(0)) {
        return TVector2<T>(T(0), T(0));
    }
    return TVector2<T>(vector.x / length, vector.y / length);
}

template<typename T>
TVector2<T> Projection(const TVector2<T> &vector, const TVector2<T> &onto) noexcept {
    T ontoLengthSq = LengthSquared(onto);
    if (ontoLengthSq == T(0)) {
        return TVector2<T>(T(0), T(0));
    }
    T projectionLength = Dot(vector, onto) / ontoLengthSq;
    return TVector2<T>(onto.x * projectionLength, onto.y * projectionLength);
}

template<typename T>
TVector2<T> Perpendicular(const TVector2<T> &vector) noexcept {
    return TVector2<T>(-vector.y, vector.x);
}

template<typename T>
TVector2<T> Rejection(const TVector2<T> &vector, const TVector2<T> &onto) noexcept {
    return vector - Projection(vector, onto);
}

template<typename T>
TVector2<T> Reflection(const TVector2<T> &vector, const TVector2<T> &normal) noexcept {
    return vector - T(2) * Dot(vector, normal) * normal;
}

template<typename T>
T Distance(const TVector2<T> &vector1, const TVector2<T> &vector2) noexcept {
    return Length(vector2 - vector1);
}

// 明示的インスタンス化
template TVector2<int> Lerp(const TVector2<int> &start, const TVector2<int> &end, int t) noexcept;
template TVector2<float> Lerp(const TVector2<float> &start, const TVector2<float> &end, float t) noexcept;
template TVector2<double> Lerp(const TVector2<double> &start, const TVector2<double> &end, double t) noexcept;

template TVector2<int> Slerp(const TVector2<int> &start, const TVector2<int> &end, int t) noexcept;
template TVector2<float> Slerp(const TVector2<float> &start, const TVector2<float> &end, float t) noexcept;
template TVector2<double> Slerp(const TVector2<double> &start, const TVector2<double> &end, double t) noexcept;

template TVector2<int> Bezier(const TVector2<int> &p0, const TVector2<int> &p1, const TVector2<int> &p2, int t) noexcept;
template TVector2<float> Bezier(const TVector2<float> &p0, const TVector2<float> &p1, const TVector2<float> &p2, float t) noexcept;
template TVector2<double> Bezier(const TVector2<double> &p0, const TVector2<double> &p1, const TVector2<double> &p2, double t) noexcept;

template TVector2<int> CatmullRomInterpolation(const TVector2<int> &p0, const TVector2<int> &p1, const TVector2<int> &p2, const TVector2<int> &p3, int t) noexcept;
template TVector2<float> CatmullRomInterpolation(const TVector2<float> &p0, const TVector2<float> &p1, const TVector2<float> &p2, const TVector2<float> &p3, float t) noexcept;
template TVector2<double> CatmullRomInterpolation(const TVector2<double> &p0, const TVector2<double> &p1, const TVector2<double> &p2, const TVector2<double> &p3, double t) noexcept;

template TVector2<int> CatmullRomPosition(const std::vector<TVector2<int>> &points, int t, bool isLoop);
template TVector2<float> CatmullRomPosition(const std::vector<TVector2<float>> &points, float t, bool isLoop);
template TVector2<double> CatmullRomPosition(const std::vector<TVector2<double>> &points, double t, bool isLoop);

template constexpr int Dot(const TVector2<int> &vector1, const TVector2<int> &vector2) noexcept;
template constexpr float Dot(const TVector2<float> &vector1, const TVector2<float> &vector2) noexcept;
template constexpr double Dot(const TVector2<double> &vector1, const TVector2<double> &vector2) noexcept;

template constexpr int Cross(const TVector2<int> &vector1, const TVector2<int> &vector2) noexcept;
template constexpr float Cross(const TVector2<float> &vector1, const TVector2<float> &vector2) noexcept;
template constexpr double Cross(const TVector2<double> &vector1, const TVector2<double> &vector2) noexcept;

template int Length(const TVector2<int> &vector) noexcept;
template float Length(const TVector2<float> &vector) noexcept;
template double Length(const TVector2<double> &vector) noexcept;

template constexpr int LengthSquared(const TVector2<int> &vector) noexcept;
template constexpr float LengthSquared(const TVector2<float> &vector) noexcept;
template constexpr double LengthSquared(const TVector2<double> &vector) noexcept;

template TVector2<int> Normalize(const TVector2<int> &vector);
template TVector2<float> Normalize(const TVector2<float> &vector);
template TVector2<double> Normalize(const TVector2<double> &vector);

template TVector2<int> Projection(const TVector2<int> &vector, const TVector2<int> &onto) noexcept;
template TVector2<float> Projection(const TVector2<float> &vector, const TVector2<float> &onto) noexcept;
template TVector2<double> Projection(const TVector2<double> &vector, const TVector2<double> &onto) noexcept;

template TVector2<int> Perpendicular(const TVector2<int> &vector) noexcept;
template TVector2<float> Perpendicular(const TVector2<float> &vector) noexcept;
template TVector2<double> Perpendicular(const TVector2<double> &vector) noexcept;

template TVector2<int> Rejection(const TVector2<int> &vector, const TVector2<int> &onto) noexcept;
template TVector2<float> Rejection(const TVector2<float> &vector, const TVector2<float> &onto) noexcept;
template TVector2<double> Rejection(const TVector2<double> &vector, const TVector2<double> &onto) noexcept;

template TVector2<int> Reflection(const TVector2<int> &vector, const TVector2<int> &normal) noexcept;
template TVector2<float> Reflection(const TVector2<float> &vector, const TVector2<float> &normal) noexcept;
template TVector2<double> Reflection(const TVector2<double> &vector, const TVector2<double> &normal) noexcept;

template int Distance(const TVector2<int> &vector1, const TVector2<int> &vector2) noexcept;
template float Distance(const TVector2<float> &vector1, const TVector2<float> &vector2) noexcept;
template double Distance(const TVector2<double> &vector1, const TVector2<double> &vector2) noexcept;

} // namespace TMathUtils
} // namespace KashipanEngine