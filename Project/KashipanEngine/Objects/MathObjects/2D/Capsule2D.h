#pragma once

#include "Math/Vector2.h"

namespace KashipanEngine {
namespace Math {

struct Capsule2D final {
    Vector2 start{0.0f, 0.0f};
    Vector2 end{0.0f, 0.0f};
    float radius = 0.0f;
};

} // namespace Math
} // namespace KashipanEngine