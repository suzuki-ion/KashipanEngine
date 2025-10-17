#pragma once

// 既存のMath構造体
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix3x3.h"
#include "Matrix4x4.h"

// テンプレート版のMath構造体
#include "TVector2.h"
#include "TVector3.h"
#include "TVector4.h"
#include "TMatrix3x3.h"
#include "TMatrix4x4.h"

// 既存のコードとの互換性のため、float版をデフォルトの型エイリアスとして定義
namespace KashipanEngine {

// int型の別名（整数演算用）
using Vector2i = TVector2<int>;
using Vector3i = TVector3<int>;
using Vector4i = TVector4<int>;
using Matrix3x3i = TMatrix3x3<int>;
using Matrix4x4i = TMatrix4x4<int>;

// float型の別名（既存のコードとの互換性を保つため）
using Vector2f = TVector2<float>;
using Vector3f = TVector3<float>;
using Vector4f = TVector4<float>;
using Matrix3x3f = TMatrix3x3<float>;
using Matrix4x4f = TMatrix4x4<float>;

// double型の別名（高精度計算用）
using Vector2d = TVector2<double>;
using Vector3d = TVector3<double>;
using Vector4d = TVector4<double>;
using Matrix3x3d = TMatrix3x3<double>;
using Matrix4x4d = TMatrix4x4<double>;

} // namespace KashipanEngine