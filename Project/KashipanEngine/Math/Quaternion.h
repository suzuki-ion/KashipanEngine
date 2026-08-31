#pragma once
#include <cmath>

#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"

struct Quaternion final {
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

    static Quaternion MakeRotateEuler(const Vector3 &euler) noexcept {
        Quaternion q;
        Quaternion qx = q.MakeRotateAxisAngle(Vector3(1.0f, 0.0f, 0.0f), euler.x);
        Quaternion qy = q.MakeRotateAxisAngle(Vector3(0.0f, 1.0f, 0.0f), euler.y);
        Quaternion qz = q.MakeRotateAxisAngle(Vector3(0.0f, 0.0f, 1.0f), euler.z);
        return (qx * qy * qz).Normalize();
    }

    static Quaternion MakeRotateAxisAngle(const Vector3 &axis, float angleRad) noexcept {
        float halfAngle = angleRad * 0.5f;
        float s = std::sin(halfAngle);
        float c = std::cos(halfAngle);
        return Quaternion(axis.x * s, axis.y * s, axis.z * s, c);
    }

    /// @brief 回転行列（スケール成分を含まない、行ベクトル規約: v' = v * M）からクォータニオンを生成する
    /// @details オイラー角を経由すると（ジンバルロックや ImGuizmo 側との回転順の不一致により）
    ///          ドラッグ操作中に回転が不安定になるため、行列から直接変換する必要がある場合に使う。
    static Quaternion MakeFromRotationMatrix(const Matrix4x4 &m) noexcept {
        const float m00 = m.m[0][0], m01 = m.m[0][1], m02 = m.m[0][2];
        const float m10 = m.m[1][0], m11 = m.m[1][1], m12 = m.m[1][2];
        const float m20 = m.m[2][0], m21 = m.m[2][1], m22 = m.m[2][2];
        const float trace = m00 + m11 + m22;

        Quaternion q;
        if (trace > 0.0f) {
            float s = std::sqrt(trace + 1.0f) * 2.0f; // s = 4w
            q.w = 0.25f * s;
            q.x = (m12 - m21) / s;
            q.y = (m20 - m02) / s;
            q.z = (m01 - m10) / s;
        } else if (m00 > m11 && m00 > m22) {
            float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f; // s = 4x
            q.w = (m12 - m21) / s;
            q.x = 0.25f * s;
            q.y = (m01 + m10) / s;
            q.z = (m02 + m20) / s;
        } else if (m11 > m22) {
            float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f; // s = 4y
            q.w = (m20 - m02) / s;
            q.x = (m01 + m10) / s;
            q.y = 0.25f * s;
            q.z = (m12 + m21) / s;
        } else {
            float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f; // s = 4z
            q.w = (m01 - m10) / s;
            q.x = (m02 + m20) / s;
            q.y = (m12 + m21) / s;
            q.z = 0.25f * s;
        }
        return q.Normalize();
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
    Vector3 MakeEuler() {
        // MakeRotateMatrix()は行ベクトル規約（v' = v * M）の回転行列を返すが、
        // 以下の抽出式は列ベクトル規約（v' = M * v）の回転行列を前提としたものである。
        // 両者は転置の関係にあるため、mat.m[i][j]の代わりにmat.m[j][i]（転置後の成分）を
        // 参照することで、MakeRotateEuler()に渡した角度をそのまま復元できるようにしている
        // （転置を取らずに実装すると、往復変換のたびに角度の符号が反転してしまうバグになる）。
        Matrix4x4 mat = MakeRotateMatrix();
        float rotX, rotY, rotZ;

        float sy = mat.m[2][0];
        if (sy > 0.9999f) {
            rotY = 3.14159265f * 0.5f;
            rotX = std::atan2(mat.m[0][1], mat.m[1][1]);
            rotZ = 0.0f;
        } else if (sy < -0.9999f) {
            rotY = -3.14159265f * 0.5f;
            rotX = std::atan2(-mat.m[0][1], mat.m[1][1]);
            rotZ = 0.0f;
        } else {
            rotY = std::asin(sy);
            rotX = std::atan2(-mat.m[2][1], mat.m[2][2]);
            rotZ = std::atan2(-mat.m[1][0], mat.m[0][0]);
        }
        return Vector3(rotX, rotY, rotZ);
    }

    float x;
    float y;
    float z;
    float w;
};