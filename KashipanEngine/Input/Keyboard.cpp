#include "Input/Keyboard.h"

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include <cassert>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace KashipanEngine {
namespace {
IDirectInputDevice8* sKeyboardDevice = nullptr;
} // namespace

Keyboard::Keyboard() = default;

Keyboard::~Keyboard() {
    Finalize();
}

void Keyboard::Initialize(HINSTANCE hInstance, HWND hwnd) {
    assert(hInstance);
    assert(hwnd);

    IDirectInput8* directInput = nullptr;
    HRESULT hr = DirectInput8Create(
        hInstance,
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        reinterpret_cast<void**>(&directInput),
        nullptr);
    assert(SUCCEEDED(hr));

    hr = directInput->CreateDevice(GUID_SysKeyboard, &sKeyboardDevice, nullptr);
    assert(SUCCEEDED(hr));

    hr = sKeyboardDevice->SetDataFormat(&c_dfDIKeyboard);
    assert(SUCCEEDED(hr));

    hr = sKeyboardDevice->SetCooperativeLevel(
        hwnd,
        DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    assert(SUCCEEDED(hr));

    directInput->Release();

    current_.fill(0);
    previous_.fill(0);
}

void Keyboard::Finalize() {
    if (sKeyboardDevice) {
        sKeyboardDevice->Unacquire();
        sKeyboardDevice->Release();
        sKeyboardDevice = nullptr;
    }
}

void Keyboard::Update() {
    previous_ = current_;

    if (!sKeyboardDevice) {
        current_.fill(0);
        return;
    }

    sKeyboardDevice->Acquire();
    sKeyboardDevice->GetDeviceState(static_cast<DWORD>(current_.size()), current_.data());
}

bool Keyboard::IsDown(int key) const {
    return (key >= 0 && key < static_cast<int>(current_.size())) ? ((current_[key] & 0x80) != 0) : false;
}

bool Keyboard::WasDown(int key) const {
    return (key >= 0 && key < static_cast<int>(previous_.size())) ? ((previous_[key] & 0x80) != 0) : false;
}

bool Keyboard::IsTrigger(int key) const {
    return IsDown(key) && !WasDown(key);
}

bool Keyboard::IsRelease(int key) const {
    return !IsDown(key) && WasDown(key);
}

} // namespace KashipanEngine
