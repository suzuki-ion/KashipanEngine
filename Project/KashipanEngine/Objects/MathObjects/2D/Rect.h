#pragma once

#include "Math/Vector2.h"

namespace KashipanEngine {
namespace Math {

struct Rect final {
    Vector2 center{0.0f, 0.0f};
    Vector2 halfSize{0.0f, 0.0f};
};

} // namespace Math
} // namespace KashipanEngine
