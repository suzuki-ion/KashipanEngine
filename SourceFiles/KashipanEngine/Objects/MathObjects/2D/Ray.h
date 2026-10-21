#pragma once

#include "Math/Vector2.h"

namespace KashipanEngine {
namespace Math {

struct Ray2D final {
    Vector2 origin{0.0f, 0.0f};
    Vector2 direction{1.0f, 0.0f};
};

} // namespace Math
} // namespace KashipanEngine
