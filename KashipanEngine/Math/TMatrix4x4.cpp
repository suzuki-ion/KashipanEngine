#include "TMatrix4x4.h"
#include "TVector3.h"
#include <cmath>

namespace KashipanEngine {

// TMatrix3x3の行列式実装
template<typename T>
constexpr T TMatrix4x4<T>::TMatrix3x3::Determinant() const noexcept {
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
         - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
         + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

template<typename T>
TMatrix4x4<T> &TMatrix4x4<T>::operator+=(const TMatrix4x4<T> &matrix) noexcept {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            m[i][j] += matrix.m[i][j];
        }
    }
    return *this;
}

template<typename T>
TMatrix4x4<T> &TMatrix4x4<T>::operator-=(const TMatrix4x4<T> &matrix) noexcept {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            m[i][j] -= matrix.m[i][j];
        }
    }
    return *this;
}

template<typename T>
TMatrix4x4<T> &TMatrix4x4<T>::operator*=(const T scalar) noexcept {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            m[i][j] *= scalar;
        }
    }
    return *this;
}

template<typename T>
TMatrix4x4<T> &TMatrix4x4<T>::operator*=(const TMatrix4x4<T> &matrix) noexcept {
    TMatrix4x4<T> result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[i][j] = T(0);
            for (int k = 0; k < 4; ++k) {
                result.m[i][j] += m[i][k] * matrix.m[k][j];
            }
        }
    }
    *this = result;
    return *this;
}

template<typename T>
constexpr const TMatrix4x4<T> TMatrix4x4<T>::operator+(const TMatrix4x4<T> &matrix) const noexcept {
    TMatrix4x4<T> result = *this;
    result += matrix;
    return result;
}

template<typename T>
constexpr const TMatrix4x4<T> TMatrix4x4<T>::operator-(const TMatrix4x4<T> &matrix) const noexcept {
    TMatrix4x4<T> result = *this;
    result -= matrix;
    return result;
}

template<typename T>
constexpr const TMatrix4x4<T> TMatrix4x4<T>::operator*(const T scalar) const noexcept {
    TMatrix4x4<T> result = *this;
    result *= scalar;
    return result;
}

template<typename T>
constexpr const TMatrix4x4<T> TMatrix4x4<T>::operator*(const TMatrix4x4<T> &matrix) const noexcept {
    TMatrix4x4<T> result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[i][j] = T(0);
            for (int k = 0; k < 4; ++k) {
                result.m[i][j] += m[i][k] * matrix.m[k][j];
            }
        }
    }
    return result;
}

template<typename T>
const TMatrix4x4<T> TMatrix4x4<T>::Identity() noexcept {
    return TMatrix4x4<T>(
        T(1), T(0), T(0), T(0),
        T(0), T(1), T(0), T(0),
        T(0), T(0), T(1), T(0),
        T(0), T(0), T(0), T(1)
    );
}

template<typename T>
const TMatrix4x4<T> TMatrix4x4<T>::Transpose() noexcept {
    return TMatrix4x4<T>(
        m[0][0], m[1][0], m[2][0], m[3][0],
        m[0][1], m[1][1], m[2][1], m[3][1],
        m[0][2], m[1][2], m[2][2], m[3][2],
        m[0][3], m[1][3], m[2][3], m[3][3]
    );
}

template<typename T>
const T TMatrix4x4<T>::Determinant() const noexcept {
    // 4x4行列の行列式計算（余因子展開）
    T det = T(0);
    for (int j = 0; j < 4; ++j) {
        // 余因子行列の作成
        TMatrix3x3 cofactor(
            m[1][(j + 1) % 4], m[1][(j + 2) % 4], m[1][(j + 3) % 4],
            m[2][(j + 1) % 4], m[2][(j + 2) % 4], m[2][(j + 3) % 4],
            m[3][(j + 1) % 4], m[3][(j + 2) % 4], m[3][(j + 3) % 4]
        );
        
        // より正確な余因子計算
        T values[9];
        int idx = 0;
        for (int row = 1; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                if (col != j) {
                    values[idx++] = m[row][col];
                }
            }
        }
        
        TMatrix3x3 properCofactor(
            values[0], values[1], values[2],
            values[3], values[4], values[5],
            values[6], values[7], values[8]
        );
        
        T sign = (j % 2 == 0) ? T(1) : T(-1);
        det += sign * m[0][j] * properCofactor.Determinant();
    }
    return det;
}

template<typename T>
TMatrix4x4<T> TMatrix4x4<T>::Inverse() const {
    T det = Determinant();
    if (det == T(0)) {
        // 特異行列の場合、単位行列を返す
        return Identity();
    }

    T invDet = T(1) / det;
    TMatrix4x4<T> result;

    // 余因子行列を計算してから転置
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            // (i,j)成分の余因子を計算
            T values[9];
            int idx = 0;
            for (int row = 0; row < 4; ++row) {
                if (row == i) continue;
                for (int col = 0; col < 4; ++col) {
                    if (col == j) continue;
                    values[idx++] = m[row][col];
                }
            }
            
            TMatrix3x3 cofactor(
                values[0], values[1], values[2],
                values[3], values[4], values[5],
                values[6], values[7], values[8]
            );
            
            T sign = ((i + j) % 2 == 0) ? T(1) : T(-1);
            result.m[j][i] = sign * cofactor.Determinant() * invDet; // 転置も同時に行う
        }
    }

    return result;
}

template<typename T>
void TMatrix4x4<T>::MakeIdentity() noexcept {
    *this = Identity();
}

template<typename T>
void TMatrix4x4<T>::MakeTranspose() noexcept {
    *this = Transpose();
}

template<typename T>
void TMatrix4x4<T>::MakeInverse() noexcept {
    *this = Inverse();
}

template<typename T>
void TMatrix4x4<T>::MakeTranslate(const TVector3<T> &translate) noexcept {
    *this = TMatrix4x4<T>(
        T(1), T(0), T(0), translate.x,
        T(0), T(1), T(0), translate.y,
        T(0), T(0), T(1), translate.z,
        T(0), T(0), T(0), T(1)
    );
}

template<typename T>
void TMatrix4x4<T>::MakeScale(const TVector3<T> &scale) noexcept {
    *this = TMatrix4x4<T>(
        scale.x, T(0), T(0), T(0),
        T(0), scale.y, T(0), T(0),
        T(0), T(0), scale.z, T(0),
        T(0), T(0), T(0), T(1)
    );
}

template<typename T>
void TMatrix4x4<T>::MakeRotate(const TVector3<T> &rotate) noexcept {
    MakeRotate(rotate.x, rotate.y, rotate.z);
}

template<typename T>
void TMatrix4x4<T>::MakeRotate(const T radianX, const T radianY, const T radianZ) noexcept {
    TMatrix4x4<T> rotX, rotY, rotZ;
    
    rotX.MakeRotateX(radianX);
    rotY.MakeRotateY(radianY);
    rotZ.MakeRotateZ(radianZ);
    
    *this = rotZ * rotY * rotX;
}

template<typename T>
void TMatrix4x4<T>::MakeRotateX(const T radian) noexcept {
    if constexpr (std::is_floating_point_v<T>) {
        T cos_r = std::cos(radian);
        T sin_r = std::sin(radian);
        *this = TMatrix4x4<T>(
            T(1), T(0), T(0), T(0),
            T(0), cos_r, -sin_r, T(0),
            T(0), sin_r, cos_r, T(0),
            T(0), T(0), T(0), T(1)
        );
    } else {
        double cos_r = std::cos(static_cast<double>(radian));
        double sin_r = std::sin(static_cast<double>(radian));
        *this = TMatrix4x4<T>(
            T(1), T(0), T(0), T(0),
            T(0), static_cast<T>(cos_r), static_cast<T>(-sin_r), T(0),
            T(0), static_cast<T>(sin_r), static_cast<T>(cos_r), T(0),
            T(0), T(0), T(0), T(1)
        );
    }
}

template<typename T>
void TMatrix4x4<T>::MakeRotateY(const T radian) noexcept {
    if constexpr (std::is_floating_point_v<T>) {
        T cos_r = std::cos(radian);
        T sin_r = std::sin(radian);
        *this = TMatrix4x4<T>(
            cos_r, T(0), sin_r, T(0),
            T(0), T(1), T(0), T(0),
            -sin_r, T(0), cos_r, T(0),
            T(0), T(0), T(0), T(1)
        );
    } else {
        double cos_r = std::cos(static_cast<double>(radian));
        double sin_r = std::sin(static_cast<double>(radian));
        *this = TMatrix4x4<T>(
            static_cast<T>(cos_r), T(0), static_cast<T>(sin_r), T(0),
            T(0), T(1), T(0), T(0),
            static_cast<T>(-sin_r), T(0), static_cast<T>(cos_r), T(0),
            T(0), T(0), T(0), T(1)
        );
    }
}

template<typename T>
void TMatrix4x4<T>::MakeRotateZ(const T radian) noexcept {
    if constexpr (std::is_floating_point_v<T>) {
        T cos_r = std::cos(radian);
        T sin_r = std::sin(radian);
        *this = TMatrix4x4<T>(
            cos_r, -sin_r, T(0), T(0),
            sin_r, cos_r, T(0), T(0),
            T(0), T(0), T(1), T(0),
            T(0), T(0), T(0), T(1)
        );
    } else {
        double cos_r = std::cos(static_cast<double>(radian));
        double sin_r = std::sin(static_cast<double>(radian));
        *this = TMatrix4x4<T>(
            static_cast<T>(cos_r), static_cast<T>(-sin_r), T(0), T(0),
            static_cast<T>(sin_r), static_cast<T>(cos_r), T(0), T(0),
            T(0), T(0), T(1), T(0),
            T(0), T(0), T(0), T(1)
        );
    }
}

template<typename T>
void TMatrix4x4<T>::MakeAffine(const TVector3<T> &scale, const TVector3<T> &rotate, const TVector3<T> &translate) noexcept {
    TMatrix4x4<T> scaleMatrix, rotateMatrix, translateMatrix;
    
    scaleMatrix.MakeScale(scale);
    rotateMatrix.MakeRotate(rotate);
    translateMatrix.MakeTranslate(translate);
    
    *this = translateMatrix * rotateMatrix * scaleMatrix;
}

template<typename T>
void TMatrix4x4<T>::MakeViewMatrix(const TVector3<T> &eyePos, const TVector3<T> &targetPos, const TVector3<T> &upVector) noexcept {
    TVector3<T> zAxis = (eyePos - targetPos).Normalize();
    TVector3<T> xAxis = upVector.Cross(zAxis).Normalize();
    TVector3<T> yAxis = zAxis.Cross(xAxis);
    
    *this = TMatrix4x4<T>(
        xAxis.x, yAxis.x, zAxis.x, T(0),
        xAxis.y, yAxis.y, zAxis.y, T(0),
        xAxis.z, yAxis.z, zAxis.z, T(0),
        -xAxis.Dot(eyePos), -yAxis.Dot(eyePos), -zAxis.Dot(eyePos), T(1)
    );
}

template<typename T>
void TMatrix4x4<T>::MakePerspectiveFovMatrix(const T fovY, const T aspectRatio, const T nearClip, const T farClip) noexcept {
    if constexpr (std::is_floating_point_v<T>) {
        T tanHalfFov = std::tan(fovY / T(2));
        T range = farClip - nearClip;
        
        *this = TMatrix4x4<T>(
            T(1) / (aspectRatio * tanHalfFov), T(0), T(0), T(0),
            T(0), T(1) / tanHalfFov, T(0), T(0),
            T(0), T(0), -(farClip + nearClip) / range, -T(2) * farClip * nearClip / range,
            T(0), T(0), T(-1), T(0)
        );
    } else {
        double tanHalfFov = std::tan(static_cast<double>(fovY) / 2.0);
        double range = static_cast<double>(farClip - nearClip);
        
        *this = TMatrix4x4<T>(
            static_cast<T>(1.0 / (static_cast<double>(aspectRatio) * tanHalfFov)), T(0), T(0), T(0),
            T(0), static_cast<T>(1.0 / tanHalfFov), T(0), T(0),
            T(0), T(0), static_cast<T>(-(static_cast<double>(farClip + nearClip)) / range), static_cast<T>(-2.0 * static_cast<double>(farClip) * static_cast<double>(nearClip) / range),
            T(0), T(0), T(-1), T(0)
        );
    }
}

template<typename T>
void TMatrix4x4<T>::MakeOrthographicMatrix(const T left, const T top, const T right, const T bottom, const T nearClip, const T farClip) noexcept {
    T width = right - left;
    T height = top - bottom;
    T depth = farClip - nearClip;
    
    *this = TMatrix4x4<T>(
        T(2) / width, T(0), T(0), -(right + left) / width,
        T(0), T(2) / height, T(0), -(top + bottom) / height,
        T(0), T(0), T(-2) / depth, -(farClip + nearClip) / depth,
        T(0), T(0), T(0), T(1)
    );
}

template<typename T>
void TMatrix4x4<T>::MakeViewportMatrix(const T left, const T top, const T width, const T height, const T minDepth, const T maxDepth) noexcept {
    T halfWidth = width / T(2);
    T halfHeight = height / T(2);
    T depthRange = maxDepth - minDepth;
    
    *this = TMatrix4x4<T>(
        halfWidth, T(0), T(0), left + halfWidth,
        T(0), -halfHeight, T(0), top + halfHeight,
        T(0), T(0), depthRange, minDepth,
        T(0), T(0), T(0), T(1)
    );
}

// 明示的インスタンス化
template struct TMatrix4x4<int>;
template struct TMatrix4x4<float>;
template struct TMatrix4x4<double>;

} // namespace KashipanEngine