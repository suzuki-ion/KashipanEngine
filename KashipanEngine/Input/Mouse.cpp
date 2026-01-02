#include "Input/Mouse.h"

#include <GameInput.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>

#include "Core/Window.h"

#pragma comment(lib, "gameinput.lib")

namespace KashipanEngine {
namespace {
IGameInput* sGameInput = nullptr;
} // namespace

Mouse::Mouse(Passkey<Input>) {}

Mouse::~Mouse() {
    Finalize();
}

void Mouse::Initialize() {
    if (!sGameInput) {
        const HRESULT hr = GameInputCreate(&sGameInput);
        assert(SUCCEEDED(hr));
    }

    currentButtons_.fill(0);
    previousButtons_.fill(0);

    currentDeltaX_ = 0;
    currentDeltaY_ = 0;
    currentWheel_ = 0;
    currentWheelValue_ = 0;
    previousDeltaX_ = 0;
    previousDeltaY_ = 0;
    previousWheel_ = 0;
    previousWheelValue_ = 0;

    GetCursorPos(&currentPosScreen);
    previousPosScreen = currentPosScreen;
    prevClientPosByWindow_.clear();

    initialized_ = true;
}

void Mouse::Finalize() {
    initialized_ = false;

    currentButtons_.fill(0);
    previousButtons_.fill(0);

    currentDeltaX_ = 0;
    currentDeltaY_ = 0;
    currentWheel_ = 0;
    currentWheelValue_ = 0;
    previousDeltaX_ = 0;
    previousDeltaY_ = 0;
    previousWheel_ = 0;
    previousWheelValue_ = 0;
}

void Mouse::Update() {
    previousButtons_ = currentButtons_;
    previousDeltaX_ = currentDeltaX_;
    previousDeltaY_ = currentDeltaY_;
    previousWheel_ = currentWheel_;
    previousWheelValue_ = currentWheelValue_;

    currentButtons_.fill(0);
    currentDeltaX_ = 0;
    currentDeltaY_ = 0;
    currentWheel_ = 0;

    // カーソル位置から移動量（差分）を算出する
    previousPosScreen = currentPosScreen;
    GetCursorPos(&currentPosScreen);
    currentDeltaX_ = currentPosScreen.x - previousPosScreen.x;
    currentDeltaY_ = currentPosScreen.y - previousPosScreen.y;

    if (!initialized_ || !sGameInput) {
        return;
    }

    IGameInputReading *reading = nullptr;
    const HRESULT hr = sGameInput->GetCurrentReading(GameInputKindMouse, nullptr, &reading);
    if (FAILED(hr) || !reading) {
        return;
    }

    GameInputMouseState state{};
    if (!reading->GetMouseState(&state)) {
        return;
    }
    const std::uint32_t buttons = static_cast<std::uint32_t>(state.buttons);
    for (int i = 0; i < 8; ++i) {
        const bool down = ((buttons & (1u << i)) != 0);
        currentButtons_[i] = down ? 0x80 : 0;
    }

    const auto clampToInt = [](int64_t v) -> int {
        if (v > static_cast<int64_t>(std::numeric_limits<int>::max())) return std::numeric_limits<int>::max();
        if (v < static_cast<int64_t>(std::numeric_limits<int>::min())) return std::numeric_limits<int>::min();
        return static_cast<int>(v);
        };

    // ホイール累積値（縦方向）
    currentWheelValue_ = clampToInt(state.wheelY);
    // フレーム差分
    currentWheel_ = currentWheelValue_ - previousWheelValue_;
    reading->Release();
}

bool Mouse::IsButtonDown(int button) const {
    if (button < 0 || button >= 8) return false;
    return (currentButtons_[button] & 0x80) != 0;
}

bool Mouse::WasButtonDown(int button) const {
    if (button < 0 || button >= 8) return false;
    return (previousButtons_[button] & 0x80) != 0;
}

bool Mouse::IsButtonTrigger(int button) const {
    return IsButtonDown(button) && !WasButtonDown(button);
}

bool Mouse::IsButtonRelease(int button) const {
    return !IsButtonDown(button) && WasButtonDown(button);
}

int Mouse::GetDeltaX() const {
    return currentDeltaX_;
}

int Mouse::GetDeltaY() const {
    return currentDeltaY_;
}

int Mouse::GetWheel() const {
    return currentWheel_;
}

int Mouse::GetWheelValue() const {
    return currentWheelValue_;
}

int Mouse::GetPrevWheel() const {
    return previousWheel_;
}

int Mouse::GetPrevWheelValue() const {
    return previousWheelValue_;
}

int Mouse::GetPrevDeltaX() const {
    return previousDeltaX_;
}

int Mouse::GetPrevDeltaY() const {
    return previousDeltaY_;
}

POINT Mouse::GetPos(HWND hwnd) const {
    POINT p = currentPosScreen;
    if (hwnd) {
        ScreenToClient(hwnd, &p);
        const auto key = GetWindowKey_(hwnd);
        if (prevClientPosByWindow_.find(key) == prevClientPosByWindow_.end()) {
            prevClientPosByWindow_[key] = p;
        }
    }
    return p;
}

POINT Mouse::GetPrevPos(HWND hwnd) const {
    if (!hwnd) {
        return previousPosScreen;
    }

    const auto key = GetWindowKey_(hwnd);

    POINT currentClient = currentPosScreen;
    ScreenToClient(hwnd, &currentClient);

    const auto it = prevClientPosByWindow_.find(key);
    POINT prevClient = (it != prevClientPosByWindow_.end()) ? it->second : currentClient;

    prevClientPosByWindow_[key] = currentClient;

    return prevClient;
}

int Mouse::GetX(HWND hwnd) const {
    return GetPos(hwnd).x;
}

int Mouse::GetY(HWND hwnd) const {
    return GetPos(hwnd).y;
}

int Mouse::GetPrevX(HWND hwnd) const {
    return GetPrevPos(hwnd).x;
}

int Mouse::GetPrevY(HWND hwnd) const {
    return GetPrevPos(hwnd).y;
}

POINT Mouse::GetPos(const Window* window) const {
    return GetPos(window ? window->GetWindowHandle() : nullptr);
}

POINT Mouse::GetPrevPos(const Window* window) const {
    return GetPrevPos(window ? window->GetWindowHandle() : nullptr);
}

int Mouse::GetX(const Window* window) const {
    return GetX(window ? window->GetWindowHandle() : nullptr);
}

int Mouse::GetY(const Window* window) const {
    return GetY(window ? window->GetWindowHandle() : nullptr);
}

int Mouse::GetPrevX(const Window* window) const {
    return GetPrevX(window ? window->GetWindowHandle() : nullptr);
}

int Mouse::GetPrevY(const Window* window) const {
    return GetPrevY(window ? window->GetWindowHandle() : nullptr);
}

} // namespace KashipanEngine
