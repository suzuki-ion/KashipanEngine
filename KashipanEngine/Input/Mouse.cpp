#include "Input/Mouse.h"

#include <Windows.h>

#include <cassert>
#include <cstring>

#include "Core/Window.h"

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace KashipanEngine {
namespace {
IDirectInputDevice8* sMouseDevice = nullptr;
} // namespace

Mouse::Mouse() = default;

Mouse::~Mouse() {
    Finalize();
}

void Mouse::Initialize(HINSTANCE hInstance) {
    assert(hInstance);

    IDirectInput8* directInput = nullptr;
    HRESULT hr = DirectInput8Create(
        hInstance,
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        reinterpret_cast<void**>(&directInput),
        nullptr);
    assert(SUCCEEDED(hr));

    hr = directInput->CreateDevice(GUID_SysMouse, &sMouseDevice, nullptr);
    assert(SUCCEEDED(hr));

    hr = sMouseDevice->SetDataFormat(&c_dfDIMouse);
    assert(SUCCEEDED(hr));

    // hwnd を要求しないため、協調レベルは全体(HWNDデスクトップ)に対して設定
    hr = sMouseDevice->SetCooperativeLevel(
        ::GetDesktopWindow(),
        DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
    assert(SUCCEEDED(hr));

    directInput->Release();

    std::memset(&currentState, 0, sizeof(currentState));
    std::memset(&previousState, 0, sizeof(previousState));

    GetCursorPos(&currentPosScreen);
    previousPosScreen = currentPosScreen;
    prevClientPosByWindow_.clear();
}

void Mouse::Finalize() {
    if (sMouseDevice) {
        sMouseDevice->Unacquire();
        sMouseDevice->Release();
        sMouseDevice = nullptr;
    }
}

void Mouse::Update() {
    previousState = currentState;

    if (sMouseDevice) {
        sMouseDevice->Acquire();
        sMouseDevice->GetDeviceState(sizeof(currentState), &currentState);
    } else {
        std::memset(&currentState, 0, sizeof(currentState));
    }

    previousPosScreen = currentPosScreen;
    GetCursorPos(&currentPosScreen);
}

bool Mouse::IsButtonDown(int button) const {
    if (button < 0 || button >= 8) return false;
    return (currentState.rgbButtons[button] & 0x80) != 0;
}

bool Mouse::WasButtonDown(int button) const {
    if (button < 0 || button >= 8) return false;
    return (previousState.rgbButtons[button] & 0x80) != 0;
}

bool Mouse::IsButtonTrigger(int button) const {
    return IsButtonDown(button) && !WasButtonDown(button);
}

bool Mouse::IsButtonRelease(int button) const {
    return !IsButtonDown(button) && WasButtonDown(button);
}

int Mouse::GetDeltaX() const {
    return static_cast<int>(currentState.lX);
}

int Mouse::GetDeltaY() const {
    return static_cast<int>(currentState.lY);
}

int Mouse::GetWheel() const {
    return static_cast<int>(currentState.lZ);
}

int Mouse::GetPrevDeltaX() const {
    return static_cast<int>(previousState.lX);
}

int Mouse::GetPrevDeltaY() const {
    return static_cast<int>(previousState.lY);
}

int Mouse::GetPrevWheel() const {
    return static_cast<int>(previousState.lZ);
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

    // まず現在クライアント座標を計算
    POINT currentClient = currentPosScreen;
    ScreenToClient(hwnd, &currentClient);

    // 前回値を取得（初回は現在値を返す）
    const auto it = prevClientPosByWindow_.find(key);
    POINT prevClient = (it != prevClientPosByWindow_.end()) ? it->second : currentClient;

    // 次回のために更新
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
