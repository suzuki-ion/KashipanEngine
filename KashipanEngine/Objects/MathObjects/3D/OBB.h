#pragma once

#include "Math/Vector3.h"
#include "Math/Matrix4x4.h"

namespace KashipanEngine {
namespace Math {

struct OBB final {
    Vector3 center{0.0f, 0.0f, 0.0f};
    Vector3 halfSize{0.0f, 0.0f, 0.0f};
    Matrix4x4 orientation = Matrix4x4::Identity();
};

} // namespace Math
} // namespace KashipanEngine