#include "Vector3.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"
#include "Math/Matrix4x4.h"
#include "Utilities/MathUtils.h"
#include <cassert>
#include <algorithm>

namespace KashipanEngine {

Vector3 Vector3::Lerp(const Vector3 &start, const Vector3 &end, float t) noexcept {
    return MathUtils::Lerp(start, end, t);
}

Vector3 Vector3::Slerp(const Vector3 &start, const Vector3 &end, float t) noexcept {
    return MathUtils::Slerp(start, end, t);
}

Vector3 Vector3::Bezier(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, float t) noexcept {
    return MathUtils::Bezier(p0, p1, p2, t);
}

Vector3 Vector3::CatmullRomInterpolation(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, float t) noexcept {
    return MathUtils::CatmullRomInterpolation(p0, p1, p2, p3, t);
}

Vector3 Vector3::CatmullRomPosition(const std::vector<Vector3> &points, float t, bool isLoop) {
    return MathUtils::CatmullRomPosition(points, t, isLoop);
}

Vector3::Vector3(const Vector2 &vector) noexcept {
    x = vector.x;
    y = vector.y;
    z = 0.0f;
}

Vector3::Vector3(const Vector4 &vector) noexcept {
    if (vector.w == 0.0f) {
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
    } else {
        x = vector.x / vector.w;
        y = vector.y / vector.w;
        z = vector.z / vector.w;
    }
}

Vector3 &Vector3::operator=(const Vector3 &vector) noexcept {
    x = vector.x;
    y = vector.y;
    z = vector.z;
    return *this;
}

Vector3 &Vector3::operator+=(const Vector3 &vector) noexcept {
    x += vector.x;
    y += vector.y;
    z += vector.z;
    return *this;
}

Vector3 &Vector3::operator-=(const Vector3 &vector) noexcept {
    x -= vector.x;
    y -= vector.y;
    z -= vector.z;
    return *this;
}

Vector3 &Vector3::operator*=(const float scalar) noexcept {
    x *= scalar;
    y *= scalar;
    z *= scalar;
    return *this;
}

Vector3 &Vector3::operator*=(const Vector3 &vector) noexcept {
    x *= vector.x;
    y *= vector.y;
    z *= vector.z;
    return *this;
}

Vector3 &Vector3::operator/=(const float scalar) {
    if (scalar == 0.0f) {
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
    } else {
        float invScalar = 1.0f / scalar;
        x *= invScalar;
        y *= invScalar;
        z *= invScalar;
    }
    return *this;
}

Vector3 &Vector3::operator/=(const Vector3 &vector) {
    if (vector.x == 0.0f) {
        x = 0.0f;
    } else {
        x /= vector.x;
    }
    if (vector.y == 0.0f) {
        y = 0.0f;
    } else {
        y /= vector.y;
    }
    if (vector.z == 0.0f) {
        z = 0.0f;
    } else {
        z /= vector.z;
    }
    return *this;
}

bool Vector3::operator==(const Vector3 &vector) const noexcept {
    return x == vector.x && y == vector.y && z == vector.z;
}

bool Vector3::operator!=(const Vector3 &vector) const noexcept {
    return x != vector.x || y != vector.y || z != vector.z;
}

float Vector3::Dot(const Vector3 &vector) const noexcept {
    return MathUtils::Dot(*this, vector);
}

Vector3 Vector3::Cross(const Vector3 &vector) const noexcept {
    return MathUtils::Cross(*this, vector);
}

float Vector3::Length() const noexcept {
    return MathUtils::Length(*this);
}

constexpr float Vector3::LengthSquared() const noexcept {
    return MathUtils::LengthSquared(*this);
}

Vector3 Vector3::Normalize() const {
    return MathUtils::Normalize(*this);
}

Vector3 Vector3::Projection(const Vector3 &vector) const noexcept {
    return MathUtils::Projection(*this, vector);
}

Vector3 Vector3::Perpendicular() const noexcept {
    return MathUtils::Perpendicular(*this);
}

Vector3 Vector3::Rejection(const Vector3 &vector) const noexcept {
    return MathUtils::Rejection(*this, vector);
}

Vector3 Vector3::Refrection(const Vector3 &normal) const noexcept {
    return MathUtils::Reflection(*this, normal);
}

float Vector3::Distance(const Vector3 &vector) const {
    return MathUtils::Distance(*this, vector);
}

Vector3 Vector3::Transform(const Matrix4x4 &mat) const noexcept {
    return MathUtils::Transform(*this, mat);
}

const Vector3 operator*(const Matrix4x4 &mat, const Vector3 &vector) noexcept {
    return Vector3(
        mat.m[0][0] * vector.x + mat.m[0][1] * vector.y + mat.m[0][2] * vector.z + mat.m[0][3],
        mat.m[1][0] * vector.x + mat.m[1][1] * vector.y + mat.m[1][2] * vector.z + mat.m[1][3],
        mat.m[2][0] * vector.x + mat.m[2][1] * vector.y + mat.m[2][2] * vector.z + mat.m[2][3]
    );
}

const Vector3 operator*(const Vector3 &vector, const Matrix4x4 &mat) noexcept {
    return Vector3(
        vector.x * mat.m[0][0] + vector.y * mat.m[1][0] + vector.z * mat.m[2][0] + mat.m[3][0],
        vector.x * mat.m[0][1] + vector.y * mat.m[1][1] + vector.z * mat.m[2][1] + mat.m[3][1],
        vector.x * mat.m[0][2] + vector.y * mat.m[1][2] + vector.z * mat.m[2][2] + mat.m[3][2]
    );
}

} // namespace KashipanEngine