#include "Input/Keyboard.h"

#include <GameInput.h>

#include <cassert>
#include <cstdint>
#include <vector>

#pragma comment(lib, "gameinput.lib")

namespace KashipanEngine {
namespace {
IGameInput* sGameInput = nullptr;

Key FromVirtualKey(std::uint8_t vk) noexcept {
    // Letters
    if (vk >= 'A' && vk <= 'Z') {
        return static_cast<Key>(static_cast<std::uint16_t>(Key::A) + static_cast<std::uint16_t>(vk - 'A'));
    }
    // Digits
    if (vk >= '0' && vk <= '9') {
        return static_cast<Key>(static_cast<std::uint16_t>(Key::D0) + static_cast<std::uint16_t>(vk - '0'));
    }

    switch (vk) {
        case VK_LEFT: return Key::Left;
        case VK_RIGHT: return Key::Right;
        case VK_UP: return Key::Up;
        case VK_DOWN: return Key::Down;

        case VK_SHIFT: return Key::Shift;
        case VK_CONTROL: return Key::Control;
        case VK_MENU: return Key::Alt;

        case VK_SPACE: return Key::Space;
        case VK_RETURN: return Key::Enter;
        case VK_ESCAPE: return Key::Escape;
        case VK_TAB: return Key::Tab;
        case VK_BACK: return Key::Backspace;

        case VK_F1: return Key::F1;
        case VK_F2: return Key::F2;
        case VK_F3: return Key::F3;
        case VK_F4: return Key::F4;
        case VK_F5: return Key::F5;
        case VK_F6: return Key::F6;
        case VK_F7: return Key::F7;
        case VK_F8: return Key::F8;
        case VK_F9: return Key::F9;
        case VK_F10: return Key::F10;
        case VK_F11: return Key::F11;
        case VK_F12: return Key::F12;

        default: return Key::Unknown;
    }
}
} // namespace

Keyboard::Keyboard(Passkey<Input>) {}

Keyboard::~Keyboard() {
    Finalize();
}

void Keyboard::Initialize() {
    if (!sGameInput) {
        const HRESULT hr = GameInputCreate(&sGameInput);
        if (FAILED(hr)) {
            assert(false);
            return;
        }
    }

    current.fill(0);
    previous.fill(0);
    initialized_ = true;
}

void Keyboard::Finalize() {
    initialized_ = false;
    current.fill(0);
    previous.fill(0);
}

size_t Keyboard::ToIndex_(Key key) noexcept {
    return static_cast<size_t>(key) & 0xFFu;
}

void Keyboard::Update() {
    previous = current;
    current.fill(0);

    if (!initialized_ || !sGameInput) {
        return;
    }

    IGameInputReading* reading = nullptr;
    const HRESULT hr = sGameInput->GetCurrentReading(GameInputKindKeyboard, nullptr, &reading);
    if (FAILED(hr) || !reading) {
        return;
    }

    const std::uint32_t keyCount = reading->GetKeyCount();
    if (keyCount > 0) {
        std::vector<GameInputKeyState> keys;
        keys.resize(keyCount);

        if (SUCCEEDED(reading->GetKeyState(keyCount, keys.data()))) {
            for (const auto& s : keys) {
                const Key key = FromVirtualKey(s.virtualKey);
                if (key == Key::Unknown) continue;
                current[ToIndex_(key)] = 0x80;
            }
        }
    }

    reading->Release();
}

bool Keyboard::IsDown(Key key) const {
    const auto idx = ToIndex_(key);
    return (idx < current.size()) ? ((current[idx] & 0x80) != 0) : false;
}

bool Keyboard::WasDown(Key key) const {
    const auto idx = ToIndex_(key);
    return (idx < previous.size()) ? ((previous[idx] & 0x80) != 0) : false;
}

bool Keyboard::IsTrigger(Key key) const {
    return IsDown(key) && !WasDown(key);
}

bool Keyboard::IsRelease(Key key) const {
    return !IsDown(key) && WasDown(key);
}

} // namespace KashipanEngine
