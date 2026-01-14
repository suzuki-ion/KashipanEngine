#pragma once

#include "Math/Vector3.h"

namespace KashipanEngine {
namespace Math {

struct Sphere final {
    Vector3 center{0.0f, 0.0f, 0.0f};
    float radius = 0.0f;
};

} // namespace Math
} // namespace KashipanEngine
