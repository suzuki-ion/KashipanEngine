#include "Input/Controller.h"

#include <Xinput.h>

#include <Windows.h>

#include <algorithm>
#include <cassert>
#include <cstring>

#pragma comment(lib, "xinput.lib")

namespace KashipanEngine {
namespace {
std::array<XINPUT_STATE, 4> sState{};
std::array<XINPUT_STATE, 4> sPreState{};
std::array<bool, 4> sConnected{};
std::array<bool, 4> sPreConnected{};
std::array<XINPUT_VIBRATION, 4> sVibration{};
} // namespace

Controller::Controller() {
    current_ = &sState;
    previous_ = &sPreState;
    connected_ = &sConnected;
    prevConnected_ = &sPreConnected;
    vibration_ = &sVibration;
}

Controller::~Controller() {
    Finalize();
}

void Controller::Initialize() {
    for (int i = 0; i < 4; ++i) {
        std::memset(&(*current_)[i], 0, sizeof(XINPUT_STATE));
        std::memset(&(*previous_)[i], 0, sizeof(XINPUT_STATE));
        (*connected_)[i] = false;
        (*prevConnected_)[i] = false;
        std::memset(&(*vibration_)[i], 0, sizeof(XINPUT_VIBRATION));
    }
}

void Controller::Finalize() {
    for (int i = 0; i < 4; ++i) {
        StopVibration(i);
    }
}

void Controller::Update() {
    *previous_ = *current_;
    *prevConnected_ = *connected_;

    for (int i = 0; i < 4; ++i) {
        std::memset(&(*current_)[i], 0, sizeof(XINPUT_STATE));
        DWORD dw = XInputGetState(i, &(*current_)[i]);
        (*connected_)[i] = (dw == ERROR_SUCCESS);

        if (!(*connected_)[i]) {
            std::memset(&(*current_)[i], 0, sizeof(XINPUT_STATE));
            continue;
        }

        auto& g = (*current_)[i].Gamepad;

        if (g.sThumbLX < +stickDeadZone_ && g.sThumbLX > -stickDeadZone_) g.sThumbLX = 0;
        if (g.sThumbLY < +stickDeadZone_ && g.sThumbLY > -stickDeadZone_) g.sThumbLY = 0;
        if (g.sThumbRX < +stickDeadZone_ && g.sThumbRX > -stickDeadZone_) g.sThumbRX = 0;
        if (g.sThumbRY < +stickDeadZone_ && g.sThumbRY > -stickDeadZone_) g.sThumbRY = 0;
    }
}

bool Controller::IsConnected(int index) const {
    return (index >= 0 && index < 4) ? (*connected_)[index] : false;
}

bool Controller::WasConnected(int index) const {
    return (index >= 0 && index < 4) ? (*prevConnected_)[index] : false;
}

bool Controller::IsButtonDown(int button, int index) const {
    if (!IsConnected(index)) return false;
    return ((*current_)[index].Gamepad.wButtons & static_cast<WORD>(button)) != 0;
}

bool Controller::WasButtonDown(int button, int index) const {
    if (!(index >= 0 && index < 4)) return false;
    return (((*previous_)[index].Gamepad.wButtons & static_cast<WORD>(button)) != 0);
}

bool Controller::IsButtonTrigger(int button, int index) const {
    return IsButtonDown(button, index) && !WasButtonDown(button, index);
}

bool Controller::IsButtonRelease(int button, int index) const {
    return !IsButtonDown(button, index) && WasButtonDown(button, index);
}

int Controller::GetLeftTrigger(int index) const {
    return (index >= 0 && index < 4) ? static_cast<int>((*current_)[index].Gamepad.bLeftTrigger) : 0;
}

int Controller::GetRightTrigger(int index) const {
    return (index >= 0 && index < 4) ? static_cast<int>((*current_)[index].Gamepad.bRightTrigger) : 0;
}

int Controller::GetLeftStickX(int index) const {
    return (index >= 0 && index < 4) ? static_cast<int>((*current_)[index].Gamepad.sThumbLX) : 0;
}

int Controller::GetLeftStickY(int index) const {
    return (index >= 0 && index < 4) ? static_cast<int>((*current_)[index].Gamepad.sThumbLY) : 0;
}

int Controller::GetRightStickX(int index) const {
    return (index >= 0 && index < 4) ? static_cast<int>((*current_)[index].Gamepad.sThumbRX) : 0;
}

int Controller::GetRightStickY(int index) const {
    return (index >= 0 && index < 4) ? static_cast<int>((*current_)[index].Gamepad.sThumbRY) : 0;
}

int Controller::GetPrevLeftTrigger(int index) const {
    return (index >= 0 && index < 4) ? static_cast<int>((*previous_)[index].Gamepad.bLeftTrigger) : 0;
}

int Controller::GetPrevRightTrigger(int index) const {
    return (index >= 0 && index < 4) ? static_cast<int>((*previous_)[index].Gamepad.bRightTrigger) : 0;
}

int Controller::GetPrevLeftStickX(int index) const {
    return (index >= 0 && index < 4) ? static_cast<int>((*previous_)[index].Gamepad.sThumbLX) : 0;
}

int Controller::GetPrevLeftStickY(int index) const {
    return (index >= 0 && index < 4) ? static_cast<int>((*previous_)[index].Gamepad.sThumbLY) : 0;
}

int Controller::GetPrevRightStickX(int index) const {
    return (index >= 0 && index < 4) ? static_cast<int>((*previous_)[index].Gamepad.sThumbRX) : 0;
}

int Controller::GetPrevRightStickY(int index) const {
    return (index >= 0 && index < 4) ? static_cast<int>((*previous_)[index].Gamepad.sThumbRY) : 0;
}

int Controller::GetDeltaLeftTrigger(int index) const {
    return GetLeftTrigger(index) - GetPrevLeftTrigger(index);
}

int Controller::GetDeltaRightTrigger(int index) const {
    return GetRightTrigger(index) - GetPrevRightTrigger(index);
}

int Controller::GetDeltaLeftStickX(int index) const {
    return GetLeftStickX(index) - GetPrevLeftStickX(index);
}

int Controller::GetDeltaLeftStickY(int index) const {
    return GetLeftStickY(index) - GetPrevLeftStickY(index);
}

int Controller::GetDeltaRightStickX(int index) const {
    return GetRightStickX(index) - GetPrevRightStickX(index);
}

int Controller::GetDeltaRightStickY(int index) const {
    return GetRightStickY(index) - GetPrevRightStickY(index);
}

void Controller::SetVibration(int index, int leftMotor, int rightMotor) {
    if (!(index >= 0 && index < 4)) return;

    if (leftMotor > -1) {
        (*vibration_)[index].wLeftMotorSpeed = static_cast<WORD>(std::clamp(leftMotor, 0, 65535));
    }
    if (rightMotor > -1) {
        (*vibration_)[index].wRightMotorSpeed = static_cast<WORD>(std::clamp(rightMotor, 0, 65535));
    }

    XInputSetState(index, &(*vibration_)[index]);
}

void Controller::StopVibration(int index) {
    if (!(index >= 0 && index < 4)) return;
    (*vibration_)[index].wLeftMotorSpeed = 0;
    (*vibration_)[index].wRightMotorSpeed = 0;
    XInputSetState(index, &(*vibration_)[index]);
}

} // namespace KashipanEngine
