#include "Vector2.h"
#include "Math/Vector3.h"
#include "Math/Matrix3x3.h"
#include "Utilities/MathUtils.h"
#include <cassert>
#include <algorithm>

Vector2 Vector2::Lerp(const Vector2 &start, const Vector2 &end, float t) noexcept {
    return KashipanEngine::MathUtils::Lerp(start, end, t);
}

Vector2 Vector2::Slerp(const Vector2 &start, const Vector2 &end, float t) noexcept {
    return KashipanEngine::MathUtils::Slerp(start, end, t);
}

Vector2 Vector2::Bezier(const Vector2 &p0, const Vector2 &p1, const Vector2 &p2, float t) noexcept {
    return KashipanEngine::MathUtils::Bezier(p0, p1, p2, t);
}

Vector2 Vector2::CatmullRomInterpolation(const Vector2 &p0, const Vector2 &p1, const Vector2 &p2, const Vector2 &p3, float t) noexcept {
    return KashipanEngine::MathUtils::CatmullRomInterpolation(p0, p1, p2, p3, t);
}

Vector2 Vector2::CatmullRomPosition(const std::vector<Vector2> &points, float t, bool isLoop) {
    return KashipanEngine::MathUtils::CatmullRomPosition(points, t, isLoop);
}

Vector2::Vector2(const Vector3 &vector) {
    x = vector.x;
    y = vector.y;
}

Vector2 &Vector2::operator=(const Vector2 &vector) {
    x = vector.x;
    y = vector.y;
    return *this;
}

Vector2 &Vector2::operator+=(const Vector2 &vector) noexcept {
    x += vector.x;
    y += vector.y;
    return *this;
}

Vector2 &Vector2::operator-=(const Vector2 &vector) noexcept {
    x -= vector.x;
    y -= vector.y;
    return *this;
}

Vector2 &Vector2::operator*=(float scalar) noexcept {
    x *= scalar;
    y *= scalar;
    return *this;
}

Vector2 &Vector2::operator*=(const Vector2 &vector) noexcept {
    x *= vector.x;
    y *= vector.y;
    return *this;
}

Vector2 &Vector2::operator/=(float scalar) {
    if (scalar == 0.0f) {
        x = 0.0f;
        y = 0.0f;
    } else {
        float inv = 1.0f / scalar;
        x *= inv;
        y *= inv;
    }
    return *this;
}

Vector2 &Vector2::operator/=(const Vector2 &vector) {
    x = (vector.x != 0.0f) ? x / vector.x : 0.0f;
    y = (vector.y != 0.0f) ? y / vector.y : 0.0f;
    return *this;
}

bool Vector2::operator==(const Vector2 &vector) const noexcept {
    return x == vector.x && y == vector.y;
}

bool Vector2::operator!=(const Vector2 &vector) const noexcept {
    return x != vector.x || y != vector.y;
}

float Vector2::Length() const noexcept {
    return KashipanEngine::MathUtils::Length(*this);
}

Vector2 Vector2::Normalize() const {
    return KashipanEngine::MathUtils::Normalize(*this);
}

Vector2 Vector2::Projection(const Vector2 &vector) const noexcept {
    return KashipanEngine::MathUtils::Projection(*this, vector);
}

Vector2 Vector2::Rejection(const Vector2 &vector) const noexcept {
    return KashipanEngine::MathUtils::Rejection(*this, vector);
}

Vector2 Vector2::Perpendicular() const noexcept {
    return KashipanEngine::MathUtils::Perpendicular(*this);
}

Vector2 Vector2::Refrection(const Vector2 &normal) const noexcept {
    return KashipanEngine::MathUtils::Reflection(*this, normal);
}

float Vector2::Distance(const Vector2 &vector) const noexcept {
    return KashipanEngine::MathUtils::Distance(*this, vector);
}
