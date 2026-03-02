#pragma once
#include <cmath>

struct Quaternion {
    static Quaternion Identity() noexcept {
        static Quaternion id(0.0f, 0.0f, 0.0f, 1.0f);
        return id;
    }
    static Quaternion Zero() noexcept {
        static Quaternion zero(0.0f, 0.0f, 0.0f, 0.0f);
        return zero;
    }

    static Quaternion Slerp(const Quaternion &q1, const Quaternion &q2, float t) noexcept {
        float dot = q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;
        Quaternion q2Copy = q2;
        if (dot < 0.0f) {
            dot = -dot;
            q2Copy.x = -q2Copy.x;
            q2Copy.y = -q2Copy.y;
            q2Copy.z = -q2Copy.z;
            q2Copy.w = -q2Copy.w;
        }
        const float dotThreshold = 0.9995f;
        if (dot > dotThreshold) {
            Quaternion result = q1 + (q2Copy - q1) * t;
            return result.Normalize();
        }
        float theta0 = std::acos(dot);
        float theta = theta0 * t;
        float sinTheta = std::sin(theta);
        float sinTheta0 = std::sin(theta0);
        float s0 = std::cos(theta) - dot * sinTheta / sinTheta0;
        float s1 = sinTheta / sinTheta0;
        return (q1 * s0) + (q2Copy * s1);
    }

    Quaternion() noexcept = default;
    constexpr Quaternion(float x, float y, float z, float w) noexcept
        : x(x), y(y), z(z), w(w)
    {}

    inline Quaternion &operator=(const Quaternion &quaternion) noexcept {
        x = quaternion.x;
        y = quaternion.y;
        z = quaternion.z;
        w = quaternion.w;
        return *this;
    }
    inline Quaternion &operator+=(const Quaternion &quaternion) noexcept {
        x += quaternion.x;
        y += quaternion.y;
        z += quaternion.z;
        w += quaternion.w;
        return *this;
    }
    inline Quaternion &operator-=(const Quaternion &quaternion) noexcept {
        x -= quaternion.x;
        y -= quaternion.y;
        z -= quaternion.z;
        w -= quaternion.w;
        return *this;
    }
    inline Quaternion &operator*=(const Quaternion &quaternion) noexcept {
        float nx = w * quaternion.x + x * quaternion.w + y * quaternion.z -  z *  quaternion.y;
        float ny = w * quaternion.y - x * quaternion.z + y * quaternion.w + z *  quaternion.x;
        float nz = w * quaternion.z + x * quaternion.y - y * quaternion.x + z *  quaternion.w;
        float nw = w * quaternion.w - x * quaternion.x - y * quaternion.y - z * quaternion.z;
        x = nx;
        y = ny;
        z = nz;
        w = nw;
        return *this;
    }
    inline Quaternion &operator*=(float scalar) noexcept {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        w *= scalar;
        return *this;
    }
    inline Quaternion &operator/=(float scalar) noexcept {
        float invScalar = 1.0f / scalar;
        x *= invScalar;
        y *= invScalar;
        z *= invScalar;
        w *= invScalar;
        return *this;
    }

    inline Quaternion operator+(const Quaternion &quaternion) const noexcept {
        return Quaternion(*this) += quaternion;
    }
    inline Quaternion operator-(const Quaternion &quaternion) const noexcept {
        return Quaternion(*this) -= quaternion;
    }
    inline Quaternion operator*(const Quaternion &quaternion) const noexcept {
        return Quaternion(*this) *= quaternion;
    }
    inline Quaternion operator*(float scalar) const noexcept {
        return Quaternion(*this) *= scalar;
    }
    inline Quaternion operator/(float scalar) const noexcept {
        return Quaternion(*this) /= scalar;
    }

    Quaternion Conjugate() const noexcept {
        return Quaternion(-x, -y, -z, w);
    }
    float NormSquared() const noexcept {
        return x * x + y * y + z * z + w * w;
    }
    float Norm() const noexcept {
        return std::sqrt(NormSquared());
    }
    Quaternion Normalize() const noexcept {
        float norm = Norm();
        if (norm == 0.0f) {
            return Quaternion::Zero();
        }
        return *this / norm;
    }
    Quaternion Inverse() const noexcept {
        float normSq = NormSquared();
        if (normSq == 0.0f) {
            return Quaternion::Zero();
        }
        return Conjugate() / normSq;
    }

    Quaternion MakeRotateAxisAngle(const Vector3 &axis, float angleRad) const noexcept {
        float halfAngle = angleRad * 0.5f;
        float s = std::sin(halfAngle);
        float c = std::cos(halfAngle);
        return Quaternion(axis.x * s, axis.y * s, axis.z * s, c);
    }

    Vector3 RotateVector(const Vector3 &vec) const noexcept {
        Quaternion vecQuat(vec.x, vec.y, vec.z, 0.0f);
        Quaternion resQuat = (*this) * vecQuat * Inverse();
        return Vector3(resQuat.x, resQuat.y, resQuat.z);
    }

    Matrix4x4 MakeRotateMatrix() const noexcept {
        Matrix4x4 mat;
        float xx = x * x;
        float yy = y * y;
        float zz = z * z;
        float xy = x * y;
        float xz = x * z;
        float yz = y * z;
        float wx = w * x;
        float wy = w * y;
        float wz = w * z;
        mat.m[0][0] = 1.0f - 2.0f * (yy + zz);
        mat.m[0][1] = 2.0f * (xy + wz);
        mat.m[0][2] = 2.0f * (xz - wy);
        mat.m[0][3] = 0.0f;
        mat.m[1][0] = 2.0f * (xy - wz);
        mat.m[1][1] = 1.0f - 2.0f * (xx + zz);
        mat.m[1][2] = 2.0f * (yz + wx);
        mat.m[1][3] = 0.0f;
        mat.m[2][0] = 2.0f * (xz + wy);
        mat.m[2][1] = 2.0f * (yz - wx);
        mat.m[2][2] = 1.0f - 2.0f * (xx + yy);
        mat.m[2][3] = 0.0f;
        mat.m[3][0] = 0.0f;
        mat.m[3][1] = 0.0f;
        mat.m[3][2] = 0.0f;
        mat.m[3][3] = 1.0f;
        return mat;
    }

    float x;
    float y;
    float z;
    float w;
};