#pragma once

#include <cstdint>

namespace KashipanEngine {

enum class Key : std::uint16_t {
    Unknown = 0,

    // 英字キー
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // 数字キー
    D0, D1, D2, D3, D4, D5, D6, D7, D8, D9,

    // 矢印キー
    Left,
    Right,
    Up,
    Down,

    // 左右を区別する修飾キー
    LeftShift,
    RightShift,
    LeftControl,
    RightControl,
    LeftAlt,
    RightAlt,

    // 左右を区別しない修飾キー
    Shift,
    Control,
    Alt,

    // 一般キー
    Space,
    Enter,
    Escape,
    Tab,
    Backspace,

    // ファンクションキー
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
};

} // namespace KashipanEngine
