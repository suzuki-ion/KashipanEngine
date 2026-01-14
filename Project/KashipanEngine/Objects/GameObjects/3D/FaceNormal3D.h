#pragma once
#include "Math/Vector3.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

inline Vector3 ToVector3(const Vector4 &v) noexcept {
    return Vector3(v.x, v.y, v.z);
}

inline Vector3 ComputeFaceNormal(const Vector4 &p0, const Vector4 &p1, const Vector4 &p2) {
    const Vector3 a = ToVector3(p1) - ToVector3(p0);
    const Vector3 b = ToVector3(p2) - ToVector3(p0);
    const Vector3 n = a.Cross(b);
    return n.Normalize();
}

} // namespace KashipanEngine
