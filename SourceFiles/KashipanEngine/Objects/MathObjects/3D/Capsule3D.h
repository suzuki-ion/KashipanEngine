#pragma once

#include "Math/Vector3.h"

namespace KashipanEngine {
namespace Math {

struct Capsule3D final {
    Vector3 start{0.0f, 0.0f, 0.0f};
    Vector3 end{0.0f, 0.0f, 0.0f};
    float radius = 0.0f;
};

} // namespace Math
} // namespace KashipanEngine
