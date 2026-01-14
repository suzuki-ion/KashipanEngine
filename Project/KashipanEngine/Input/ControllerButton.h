#pragma once

#include <cstdint>

namespace KashipanEngine {

// XInput compatible button mask values
enum class ControllerButton : std::uint16_t {
    DPadUp = 0x0001,
    DPadDown = 0x0002,
    DPadLeft = 0x0004,
    DPadRight = 0x0008,

    Start = 0x0010,
    Back = 0x0020,

    LeftThumb = 0x0040,
    RightThumb = 0x0080,

    LeftShoulder = 0x0100,
    RightShoulder = 0x0200,

    A = 0x1000,
    B = 0x2000,
    X = 0x4000,
    Y = 0x8000,
};

constexpr std::uint16_t ToMask(ControllerButton b) noexcept {
    return static_cast<std::uint16_t>(b);
}

} // namespace KashipanEngine
