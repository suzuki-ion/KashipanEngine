#pragma once

#include <cstdint>

namespace KashipanEngine {

// GameInput compatible mouse button index values
enum class MouseButton : std::uint8_t {
    Left   = 0,
    Right  = 1,
    Middle = 2,
    Button4 = 3,
    Button5 = 4,
    Button6 = 5,
    Button7 = 6,
    Button8 = 7,
};

} // namespace KashipanEngine
