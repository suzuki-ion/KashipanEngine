#pragma once

#include "Math/Vector3.h"

namespace KashipanEngine {
namespace Math {

struct Line3D final {
    Vector3 origin{0.0f, 0.0f, 0.0f};
    Vector3 direction{1.0f, 0.0f, 0.0f};
};

} // namespace Math
} // namespace KashipanEngine
