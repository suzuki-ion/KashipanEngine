#pragma once

#include "Math/Vector3.h"

namespace KashipanEngine {
namespace Math {

struct AABB final {
    Vector3 min{0.0f, 0.0f, 0.0f};
    Vector3 max{0.0f, 0.0f, 0.0f};
};

} // namespace Math
} // namespace KashipanEngine
