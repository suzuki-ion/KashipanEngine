#pragma once

#include "Math/Vector3.h"

namespace KashipanEngine {
namespace Math {

struct Plane final {
    Vector3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;
};

} // namespace Math
} // namespace KashipanEngine
