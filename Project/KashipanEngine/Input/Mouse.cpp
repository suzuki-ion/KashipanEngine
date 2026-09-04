#include "Input/Mouse.h"

#include "Debug/Logger.h"

#include <GameInput.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <wrl/client.h>

#include "Core/Window.h"

#pragma comment(lib, "gameinput.lib")

namespace KashipanEngine {
namespace {
IGameInput* sGameInput = nullptr;
} // namespace

Mouse::Mouse(Passkey<Input>) {}

Mouse::~Mouse() {
    LogScope scope;
    Finalize();
}

void Mouse::Initialize() {
    LogScope scope;
    if (!sGameInput) {
        const HRESULT hr = GameInputCreate(&sGameInput);
        if (FAILED(hr)) {
            assert(false);
            return;
        }
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
    LogScope scope;
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
    LogScope scope;
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

    Microsoft::WRL::ComPtr<IGameInputReading> reading;
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
}

bool Mouse::IsButtonDown(int button) const {
    LogScope scope;
    if (button < 0 || button >= 8) return false;
    return (currentButtons_[button] & 0x80) != 0;
}

bool Mouse::WasButtonDown(int button) const {
    LogScope scope;
    if (button < 0 || button >= 8) return false;
    return (previousButtons_[button] & 0x80) != 0;
}

bool Mouse::IsButtonTrigger(int button) const {
    LogScope scope;
    return IsButtonDown(button) && !WasButtonDown(button);
}

bool Mouse::IsButtonRelease(int button) const {
    LogScope scope;
    return !IsButtonDown(button) && WasButtonDown(button);
}

int Mouse::GetDeltaX() const {
    LogScope scope;
    return currentDeltaX_;
}

int Mouse::GetDeltaY() const {
    LogScope scope;
    return currentDeltaY_;
}

int Mouse::GetWheel() const {
    LogScope scope;
    return currentWheel_;
}

int Mouse::GetWheelValue() const {
    LogScope scope;
    return currentWheelValue_;
}

int Mouse::GetPrevWheel() const {
    LogScope scope;
    return previousWheel_;
}

int Mouse::GetPrevWheelValue() const {
    LogScope scope;
    return previousWheelValue_;
}

int Mouse::GetPrevDeltaX() const {
    LogScope scope;
    return previousDeltaX_;
}

int Mouse::GetPrevDeltaY() const {
    LogScope scope;
    return previousDeltaY_;
}

POINT Mouse::GetPos(HWND hwnd) const {
    LogScope scope;
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
    LogScope scope;
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
    LogScope scope;
    return GetPos(hwnd).x;
}

int Mouse::GetY(HWND hwnd) const {
    LogScope scope;
    return GetPos(hwnd).y;
}

int Mouse::GetPrevX(HWND hwnd) const {
    LogScope scope;
    return GetPrevPos(hwnd).x;
}

int Mouse::GetPrevY(HWND hwnd) const {
    LogScope scope;
    return GetPrevPos(hwnd).y;
}

POINT Mouse::GetPos(const Window* window) const {
    LogScope scope;
    return GetPos(window ? window->GetWindowHandle() : nullptr);
}

POINT Mouse::GetPrevPos(const Window* window) const {
    LogScope scope;
    return GetPrevPos(window ? window->GetWindowHandle() : nullptr);
}

int Mouse::GetX(const Window* window) const {
    LogScope scope;
    return GetX(window ? window->GetWindowHandle() : nullptr);
}

int Mouse::GetY(const Window* window) const {
    LogScope scope;
    return GetY(window ? window->GetWindowHandle() : nullptr);
}

int Mouse::GetPrevX(const Window* window) const {
    LogScope scope;
    return GetPrevX(window ? window->GetWindowHandle() : nullptr);
}

int Mouse::GetPrevY(const Window* window) const {
    LogScope scope;
    return GetPrevY(window ? window->GetWindowHandle() : nullptr);
}

} // namespace KashipanEngine
