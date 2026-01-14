#pragma once

#include <cstdint>

namespace KashipanEngine {

enum class Key : std::uint16_t {
    Unknown = 0,

    // Letters
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // Digits
    D0, D1, D2, D3, D4, D5, D6, D7, D8, D9,

    // Arrows
    Left,
    Right,
    Up,
    Down,

    // Modifiers (specific)
    LeftShift,
    RightShift,
    LeftControl,
    RightControl,
    LeftAlt,
    RightAlt,

    // Modifiers (generic)
    Shift,
    Control,
    Alt,

    // Common
    Space,
    Enter,
    Escape,
    Tab,
    Backspace,

    // Function keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
};

} // namespace KashipanEngine
