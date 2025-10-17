#include "TMatrix3x3.h"
#include "TVector2.h"
#include <cmath>

namespace KashipanEngine {

template<typename T>
TMatrix3x3<T> &TMatrix3x3<T>::operator+=(const TMatrix3x3<T> &matrix) noexcept {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            m[i][j] += matrix.m[i][j];
        }
    }
    return *this;
}

template<typename T>
TMatrix3x3<T> &TMatrix3x3<T>::operator-=(const TMatrix3x3<T> &matrix) noexcept {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            m[i][j] -= matrix.m[i][j];
        }
    }
    return *this;
}

template<typename T>
TMatrix3x3<T> &TMatrix3x3<T>::operator*=(T scalar) noexcept {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            m[i][j] *= scalar;
        }
    }
    return *this;
}

template<typename T>
TMatrix3x3<T> &TMatrix3x3<T>::operator*=(const TMatrix3x3<T> &matrix) noexcept {
    TMatrix3x3<T> result;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            result.m[i][j] = T(0);
            for (int k = 0; k < 3; ++k) {
                result.m[i][j] += m[i][k] * matrix.m[k][j];
            }
        }
    }
    *this = result;
    return *this;
}

template<typename T>
constexpr TMatrix3x3<T> TMatrix3x3<T>::operator+(const TMatrix3x3<T> &matrix) const noexcept {
    TMatrix3x3<T> result = *this;
    result += matrix;
    return result;
}

template<typename T>
constexpr TMatrix3x3<T> TMatrix3x3<T>::operator-(const TMatrix3x3<T> &matrix) const noexcept {
    TMatrix3x3<T> result = *this;
    result -= matrix;
    return result;
}

template<typename T>
constexpr TMatrix3x3<T> TMatrix3x3<T>::operator*(T scalar) const noexcept {
    TMatrix3x3<T> result = *this;
    result *= scalar;
    return result;
}

template<typename T>
constexpr TMatrix3x3<T> TMatrix3x3<T>::operator*(const TMatrix3x3<T> &matrix) const noexcept {
    TMatrix3x3<T> result;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            result.m[i][j] = T(0);
            for (int k = 0; k < 3; ++k) {
                result.m[i][j] += m[i][k] * matrix.m[k][j];
            }
        }
    }
    return result;
}

template<typename T>
const TMatrix3x3<T> TMatrix3x3<T>::Identity() noexcept {
    return TMatrix3x3<T>(
        T(1), T(0), T(0),
        T(0), T(1), T(0),
        T(0), T(0), T(1)
    );
}

template<typename T>
const TMatrix3x3<T> TMatrix3x3<T>::Transpose() const noexcept {
    return TMatrix3x3<T>(
        m[0][0], m[1][0], m[2][0],
        m[0][1], m[1][1], m[2][1],
        m[0][2], m[1][2], m[2][2]
    );
}

template<typename T>
const T TMatrix3x3<T>::Determinant() const noexcept {
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
         - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
         + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

template<typename T>
TMatrix3x3<T> TMatrix3x3<T>::Inverse() const {
    T det = Determinant();
    if (det == T(0)) {
        // 特異行列の場合、単位行列を返す
        return Identity();
    }

    T invDet = T(1) / det;

    TMatrix3x3<T> result;
    result.m[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet;
    result.m[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invDet;
    result.m[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet;

    result.m[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invDet;
    result.m[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet;
    result.m[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * invDet;

    result.m[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet;
    result.m[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * invDet;
    result.m[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet;

    return result;
}

template<typename T>
void TMatrix3x3<T>::MakeIdentity() noexcept {
    *this = Identity();
}

template<typename T>
void TMatrix3x3<T>::MakeTranspose() noexcept {
    *this = Transpose();
}

template<typename T>
void TMatrix3x3<T>::MakeInverse() noexcept {
    *this = Inverse();
}

template<typename T>
void TMatrix3x3<T>::MakeTranslate(const TVector2<T> &translate) noexcept {
    *this = TMatrix3x3<T>(
        T(1), T(0), translate.x,
        T(0), T(1), translate.y,
        T(0), T(0), T(1)
    );
}

template<typename T>
void TMatrix3x3<T>::MakeScale(const TVector2<T> &scale) noexcept {
    *this = TMatrix3x3<T>(
        scale.x, T(0), T(0),
        T(0), scale.y, T(0),
        T(0), T(0), T(1)
    );
}

template<typename T>
void TMatrix3x3<T>::MakeRotate(T radian) noexcept {
    if constexpr (std::is_floating_point_v<T>) {
        T cos_r = std::cos(radian);
        T sin_r = std::sin(radian);
        *this = TMatrix3x3<T>(
            cos_r, -sin_r, T(0),
            sin_r, cos_r, T(0),
            T(0), T(0), T(1)
        );
    } else {
        // 整数型の場合、近似値を使用
        double cos_r = std::cos(static_cast<double>(radian));
        double sin_r = std::sin(static_cast<double>(radian));
        *this = TMatrix3x3<T>(
            static_cast<T>(cos_r), static_cast<T>(-sin_r), T(0),
            static_cast<T>(sin_r), static_cast<T>(cos_r), T(0),
            T(0), T(0), T(1)
        );
    }
}

template<typename T>
void TMatrix3x3<T>::MakeAffine(const TVector2<T> &scale, T radian, const TVector2<T> &translate) noexcept {
    if constexpr (std::is_floating_point_v<T>) {
        T cos_r = std::cos(radian);
        T sin_r = std::sin(radian);
        *this = TMatrix3x3<T>(
            scale.x * cos_r, scale.x * (-sin_r), translate.x,
            scale.y * sin_r, scale.y * cos_r, translate.y,
            T(0), T(0), T(1)
        );
    } else {
        // 整数型の場合、近似値を使用
        double cos_r = std::cos(static_cast<double>(radian));
        double sin_r = std::sin(static_cast<double>(radian));
        *this = TMatrix3x3<T>(
            static_cast<T>(scale.x * cos_r), static_cast<T>(scale.x * (-sin_r)), translate.x,
            static_cast<T>(scale.y * sin_r), static_cast<T>(scale.y * cos_r), translate.y,
            T(0), T(0), T(1)
        );
    }
}

// 明示的インスタンス化
template struct TMatrix3x3<int>;
template struct TMatrix3x3<float>;
template struct TMatrix3x3<double>;

} // namespace KashipanEngine