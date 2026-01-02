#include "Input/Controller.h"

#include <GameInput.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>

#pragma comment(lib, "gameinput.lib")

namespace KashipanEngine {
namespace {
IGameInput* sGameInput = nullptr;

std::int16_t ApplyDeadZone(std::int16_t v, std::int16_t dz) {
    if (v < +dz && v > -dz) return 0;
    return v;
}

bool IsDeviceConnected(IGameInputDevice* device) {
    if (!device) return false;
    const auto status = device->GetDeviceStatus();
    return (status & GameInputDeviceConnected) != 0;
}

int FindDeviceIndex(const std::vector<Controller::DeviceEntry>& devices, IGameInputDevice* device) {
    for (size_t i = 0; i < devices.size(); ++i) {
        if (devices[i].device == device) return static_cast<int>(i);
    }
    return -1;
}

} // namespace

void CALLBACK OnGamepadDeviceChanged(
    GameInputCallbackToken /*callbackToken*/,
    void* context,
    IGameInputDevice* device,
    uint64_t /*timestamp*/,
    GameInputDeviceStatus currentStatus,
    GameInputDeviceStatus /*previousStatus*/) {

    if (!context || !device) return;
    auto* self = reinterpret_cast<Controller*>(context);
    self->OnDeviceChanged(device, static_cast<std::uint32_t>(currentStatus));
}

void Controller::OnDeviceChanged(void* dev, std::uint32_t currentStatus) {
    auto* device = static_cast<IGameInputDevice*>(dev);
    if (!device) return;

    const bool connectedNow = (currentStatus & static_cast<std::uint32_t>(GameInputDeviceConnected)) != 0;
    const int existing = FindDeviceIndex(devices_, device);

    if (connectedNow) {
        if (existing < 0) {
            device->AddRef();
            devices_.push_back({ device });
        }
    } else {
        if (existing >= 0) {
            auto& entry = devices_[static_cast<size_t>(existing)];
            auto* d = static_cast<IGameInputDevice*>(entry.device);
            if (d) {
                GameInputRumbleParams zero{};
                d->SetRumbleState(&zero);
                d->Release();
            }
            devices_.erase(devices_.begin() + existing);
        }
    }
}

Controller::Controller(Passkey<Input>) {}

Controller::~Controller() {
    Finalize();
}

void Controller::Initialize() {
    if (!sGameInput) {
        const HRESULT hr = GameInputCreate(&sGameInput);
        assert(SUCCEEDED(hr));
    }

    // ストレージをリセット
    devices_.clear();
    current_.clear();
    previous_.clear();
    connected_.clear();
    prevConnected_.clear();

    if (sGameInput) {
        // デバイスコールバックは列挙も兼ねる
        // すべてのゲームパッドデバイスを対象に登録する
        const HRESULT hr = sGameInput->RegisterDeviceCallback(
            nullptr,
            GameInputKindGamepad,
            GameInputDeviceAnyStatus,
            GameInputBlockingEnumeration,
            this,
            &OnGamepadDeviceChanged,
            &deviceCallbackToken_);
        assert(SUCCEEDED(hr));
    }
}

void Controller::Finalize() {
    // 先にコールバックを停止して、終了処理と競合しないようにする
    if (sGameInput && deviceCallbackToken_ != 0) {
        sGameInput->UnregisterCallback(deviceCallbackToken_, 0);
        deviceCallbackToken_ = 0;
    }

    // 振動を停止し、保持しているデバイス参照を解放する
    for (auto& e : devices_) {
        auto* d = static_cast<IGameInputDevice*>(e.device);
        if (!d) continue;
        GameInputRumbleParams zero{};
        d->SetRumbleState(&zero);
        d->Release();
        e.device = nullptr;
    }
    devices_.clear();

    current_.clear();
    previous_.clear();
    connected_.clear();
    prevConnected_.clear();
}

std::uint16_t Controller::ButtonsToXInputMask(std::uint32_t b) noexcept {
    // XInput.h を include せず互換性を保つため、値を XInput.h から複製している
    constexpr std::uint16_t X_DPAD_UP = 0x0001;
    constexpr std::uint16_t X_DPAD_DOWN = 0x0002;
    constexpr std::uint16_t X_DPAD_LEFT = 0x0004;
    constexpr std::uint16_t X_DPAD_RIGHT = 0x0008;
    constexpr std::uint16_t X_START = 0x0010;
    constexpr std::uint16_t X_BACK = 0x0020;
    constexpr std::uint16_t X_LTHUMB = 0x0040;
    constexpr std::uint16_t X_RTHUMB = 0x0080;
    constexpr std::uint16_t X_LSHOULDER = 0x0100;
    constexpr std::uint16_t X_RSHOULDER = 0x0200;
    constexpr std::uint16_t X_A = 0x1000;
    constexpr std::uint16_t X_B = 0x2000;
    constexpr std::uint16_t X_X = 0x4000;
    constexpr std::uint16_t X_Y = 0x8000;

    std::uint16_t mask = 0;

    if (b & GameInputGamepadDPadUp) mask |= X_DPAD_UP;
    if (b & GameInputGamepadDPadDown) mask |= X_DPAD_DOWN;
    if (b & GameInputGamepadDPadLeft) mask |= X_DPAD_LEFT;
    if (b & GameInputGamepadDPadRight) mask |= X_DPAD_RIGHT;

    if (b & GameInputGamepadMenu) mask |= X_START;
    if (b & GameInputGamepadView) mask |= X_BACK;

    if (b & GameInputGamepadLeftThumbstick) mask |= X_LTHUMB;
    if (b & GameInputGamepadRightThumbstick) mask |= X_RTHUMB;

    if (b & GameInputGamepadLeftShoulder) mask |= X_LSHOULDER;
    if (b & GameInputGamepadRightShoulder) mask |= X_RSHOULDER;

    if (b & GameInputGamepadA) mask |= X_A;
    if (b & GameInputGamepadB) mask |= X_B;
    if (b & GameInputGamepadX) mask |= X_X;
    if (b & GameInputGamepadY) mask |= X_Y;

    return mask;
}

void Controller::Update() {
    previous_ = current_;
    prevConnected_ = connected_;

    const size_t count = devices_.size();
    current_.assign(count, PadState{});
    connected_.assign(count, false);

    if (!sGameInput) {
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        auto* device = static_cast<IGameInputDevice*>(devices_[i].device);
        if (!device || !IsDeviceConnected(device)) {
            connected_[i] = false;
            continue;
        }

        IGameInputReading* reading = nullptr;
        const HRESULT hr = sGameInput->GetCurrentReading(GameInputKindGamepad, device, &reading);
        if (FAILED(hr) || !reading) {
            connected_[i] = false;
            continue;
        }

        GameInputGamepadState state{};
        if (SUCCEEDED(reading->GetGamepadState(&state))) {
            connected_[i] = true;

            current_[i].buttons = ButtonsToXInputMask(state.buttons);

            current_[i].leftTrigger = static_cast<std::uint8_t>(std::clamp(state.leftTrigger * 255.0f, 0.0f, 255.0f));
            current_[i].rightTrigger = static_cast<std::uint8_t>(std::clamp(state.rightTrigger * 255.0f, 0.0f, 255.0f));

            current_[i].leftX = static_cast<std::int16_t>(std::clamp(state.leftThumbstickX * 32767.0f, -32767.0f, 32767.0f));
            current_[i].leftY = static_cast<std::int16_t>(std::clamp(state.leftThumbstickY * 32767.0f, -32767.0f, 32767.0f));
            current_[i].rightX = static_cast<std::int16_t>(std::clamp(state.rightThumbstickX * 32767.0f, -32767.0f, 32767.0f));
            current_[i].rightY = static_cast<std::int16_t>(std::clamp(state.rightThumbstickY * 32767.0f, -32767.0f, 32767.0f));

            current_[i].leftX = ApplyDeadZone(current_[i].leftX, stickDeadZone_);
            current_[i].leftY = ApplyDeadZone(current_[i].leftY, stickDeadZone_);
            current_[i].rightX = ApplyDeadZone(current_[i].rightX, stickDeadZone_);
            current_[i].rightY = ApplyDeadZone(current_[i].rightY, stickDeadZone_);
        } else {
            connected_[i] = false;
        }

        reading->Release();
    }

    if (previous_.size() != current_.size()) {
        previous_.resize(current_.size());
    }
    if (prevConnected_.size() != connected_.size()) {
        prevConnected_.resize(connected_.size(), false);
    }
}

bool Controller::IsConnected(int index) const {
    return (index >= 0 && index < static_cast<int>(connected_.size())) ? connected_[static_cast<size_t>(index)] : false;
}

bool Controller::WasConnected(int index) const {
    return (index >= 0 && index < static_cast<int>(prevConnected_.size())) ? prevConnected_[static_cast<size_t>(index)] : false;
}

bool Controller::IsButtonDown(int button, int index) const {
    if (!IsConnected(index)) return false;
    return (current_[static_cast<size_t>(index)].buttons & static_cast<std::uint16_t>(button)) != 0;
}

bool Controller::WasButtonDown(int button, int index) const {
    if (!(index >= 0 && index < static_cast<int>(previous_.size()))) return false;
    return (previous_[static_cast<size_t>(index)].buttons & static_cast<std::uint16_t>(button)) != 0;
}

bool Controller::IsButtonTrigger(int button, int index) const {
    return IsButtonDown(button, index) && !WasButtonDown(button, index);
}

bool Controller::IsButtonRelease(int button, int index) const {
    return !IsButtonDown(button, index) && WasButtonDown(button, index);
}

int Controller::GetLeftTrigger(int index) const {
    return (index >= 0 && index < static_cast<int>(current_.size())) ? static_cast<int>(current_[static_cast<size_t>(index)].leftTrigger) : 0;
}

int Controller::GetRightTrigger(int index) const {
    return (index >= 0 && index < static_cast<int>(current_.size())) ? static_cast<int>(current_[static_cast<size_t>(index)].rightTrigger) : 0;
}

int Controller::GetLeftStickX(int index) const {
    return (index >= 0 && index < static_cast<int>(current_.size())) ? static_cast<int>(current_[static_cast<size_t>(index)].leftX) : 0;
}

int Controller::GetLeftStickY(int index) const {
    return (index >= 0 && index < static_cast<int>(current_.size())) ? static_cast<int>(current_[static_cast<size_t>(index)].leftY) : 0;
}

int Controller::GetRightStickX(int index) const {
    return (index >= 0 && index < static_cast<int>(current_.size())) ? static_cast<int>(current_[static_cast<size_t>(index)].rightX) : 0;
}

int Controller::GetRightStickY(int index) const {
    return (index >= 0 && index < static_cast<int>(current_.size())) ? static_cast<int>(current_[static_cast<size_t>(index)].rightY) : 0;
}

int Controller::GetPrevLeftTrigger(int index) const {
    return (index >= 0 && index < static_cast<int>(previous_.size())) ? static_cast<int>(previous_[static_cast<size_t>(index)].leftTrigger) : 0;
}

int Controller::GetPrevRightTrigger(int index) const {
    return (index >= 0 && index < static_cast<int>(previous_.size())) ? static_cast<int>(previous_[static_cast<size_t>(index)].rightTrigger) : 0;
}

int Controller::GetPrevLeftStickX(int index) const {
    return (index >= 0 && index < static_cast<int>(previous_.size())) ? static_cast<int>(previous_[static_cast<size_t>(index)].leftX) : 0;
}

int Controller::GetPrevLeftStickY(int index) const {
    return (index >= 0 && index < static_cast<int>(previous_.size())) ? static_cast<int>(previous_[static_cast<size_t>(index)].leftY) : 0;
}

int Controller::GetPrevRightStickX(int index) const {
    return (index >= 0 && index < static_cast<int>(previous_.size())) ? static_cast<int>(previous_[static_cast<size_t>(index)].rightX) : 0;
}

int Controller::GetPrevRightStickY(int index) const {
    return (index >= 0 && index < static_cast<int>(previous_.size())) ? static_cast<int>(previous_[static_cast<size_t>(index)].rightY) : 0;
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
    if (!(index >= 0 && index < static_cast<int>(devices_.size()))) return;
    auto* device = static_cast<IGameInputDevice*>(devices_[static_cast<size_t>(index)].device);
    if (!device) return;

    const auto toFloat = [](int v) {
        if (v < 0) return -1.0f;
        return std::clamp(static_cast<float>(v) / 65535.0f, 0.0f, 1.0f);
    };

    GameInputRumbleParams params{};
    const float l = toFloat(leftMotor);
    const float r = toFloat(rightMotor);

    params.lowFrequency = (l >= 0.0f) ? l : 0.0f;
    params.highFrequency = (r >= 0.0f) ? r : 0.0f;
    params.leftTrigger = 0.0f;
    params.rightTrigger = 0.0f;

    device->SetRumbleState(&params);
}

void Controller::StopVibration(int index) {
    if (!(index >= 0 && index < static_cast<int>(devices_.size()))) return;
    auto* device = static_cast<IGameInputDevice*>(devices_[static_cast<size_t>(index)].device);
    if (!device) return;

    GameInputRumbleParams zero{};
    device->SetRumbleState(&zero);
}

} // namespace KashipanEngine
