#pragma once

#include "Math/Vector3.h"
#include "Math/Matrix3x3.h"

namespace KashipanEngine {
namespace Math {

struct OBB final {
    Vector3 center{0.0f, 0.0f, 0.0f};
    Vector3 halfSize{0.0f, 0.0f, 0.0f};
    Matrix3x3 orientation = Matrix3x3::Identity();
};

} // namespace Math
} // namespace KashipanEngine