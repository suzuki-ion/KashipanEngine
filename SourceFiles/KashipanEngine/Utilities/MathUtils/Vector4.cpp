#include "Vector4.h"
#include "Math/Vector4.h"

namespace KashipanEngine {
namespace MathUtils {

Vector4 Lerp(const Vector4 &start, const Vector4 &end, float t) noexcept {
    return start * (1.0f - t) + end * t;
}

} // namespace MathUtils
} // namespace KashipanEngine