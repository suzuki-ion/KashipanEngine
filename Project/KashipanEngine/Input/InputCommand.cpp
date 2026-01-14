#include "Input/InputCommand.h"

#include "Input/Input.h"
#include "Input/Keyboard.h"
#include "Input/Mouse.h"
#include "Input/Controller.h"

#include <algorithm>
#include <cmath>

namespace KashipanEngine {

namespace {
float Clamp01(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

float Clamp11(float v) {
    return std::clamp(v, -1.0f, 1.0f);
}

bool EvaluateDigital(bool down, bool trigger, bool release, InputCommand::InputState state) {
    switch (state) {
    case InputCommand::InputState::Down: return down;
    case InputCommand::InputState::Trigger: return trigger;
    case InputCommand::InputState::Release: return release;
    default: return false;
    }
}

float NormalizeTrigger255(int v) {
    return Clamp01(static_cast<float>(v) / 255.0f);
}

float NormalizeStickInt16(int v) {
    if (v >= 0) return Clamp11(static_cast<float>(v) / 32767.0f);
    return Clamp11(static_cast<float>(v) / 32768.0f);
}

float NormalizeTriggerDelta255(int dv) {
    return Clamp11(static_cast<float>(dv) / 255.0f);
}

float NormalizeStickDeltaInt16(int dv) {
    return Clamp11(static_cast<float>(dv) / 32767.0f);
}

static bool AxisTriggered(float v, float threshold) {
    return std::abs(v) > threshold;
}
} // namespace

InputCommand::InputCommand(Passkey<GameEngine>, const Input* input) : input_(input) {}

void InputCommand::Clear() {
    bindings_.clear();
}

void InputCommand::RegisterCommand(const std::string& action, KeyboardKey key, InputState state, bool invertValue) {
    if (action.empty()) return;
    Binding b{};
    b.kind = DeviceKind::Keyboard;
    b.state = state;
    b.key = key.value;
    b.invertValue = invertValue;
    bindings_[action].push_back(b);
}

void InputCommand::RegisterCommand(const std::string& action, MouseButton button, InputState state, bool invertValue) {
    if (action.empty()) return;
    Binding b{};
    b.kind = DeviceKind::MouseButton;
    b.state = state;
    b.code = button.value;
    b.invertValue = invertValue;
    bindings_[action].push_back(b);
}

void InputCommand::RegisterCommand(const std::string& action, MouseAxis axis, void* hwnd, float threshold, bool invertValue) {
    if (action.empty()) return;
    Binding b{};
    b.kind = DeviceKind::MouseAxis;
    b.mouseAxis = axis;
    b.mouseSpace = (hwnd != nullptr) ? MouseSpace::Client : MouseSpace::Screen;
    b.hwnd = hwnd;
    b.threshold = threshold;
    b.invertValue = invertValue;
    bindings_[action].push_back(b);
}

void InputCommand::RegisterCommand(const std::string& action, ControllerButton button, InputState state, int controllerIndex, bool invertValue) {
    if (action.empty()) return;
    Binding b{};
    b.kind = DeviceKind::ControllerButton;
    b.state = state;
    b.controllerButton = button;
    b.controllerIndex = controllerIndex;
    b.invertValue = invertValue;
    bindings_[action].push_back(b);
}

void InputCommand::RegisterCommand(const std::string& action, ControllerAnalog analog, InputState state, int controllerIndex, float threshold, bool invertValue) {
    if (action.empty()) return;
    Binding b{};
    b.kind = DeviceKind::ControllerAnalog;
    b.state = state;
    b.code = static_cast<int>(analog);
    b.controllerIndex = controllerIndex;
    b.threshold = threshold;
    b.invertValue = invertValue;
    bindings_[action].push_back(b);
}

void InputCommand::RegisterCommand(const std::string& action, ControllerAnalog analog, int controllerIndex, float threshold, bool invertValue) {
    if (action.empty()) return;
    Binding b{};
    b.kind = DeviceKind::ControllerAnalogDelta;
    b.code = static_cast<int>(analog);
    b.controllerIndex = controllerIndex;
    b.threshold = threshold;
    b.invertValue = invertValue;
    bindings_[action].push_back(b);
}

InputCommand::ReturnInfo InputCommand::Evaluate(const std::string& action) const {
    if (!input_) return MakeReturnInfo(false, 0.0f);

    auto it = bindings_.find(action);
    if (it == bindings_.end() || it->second.empty()) {
        return MakeReturnInfo(false, 0.0f);
    }

    bool anyTriggered = false;
    float value = 0.0f;

    for (const auto& b : it->second) {
        ReturnInfo ri = EvaluateBinding(b);
        anyTriggered = anyTriggered || ri.Triggered();
        if (ri.Triggered()) {
            value = std::clamp(value + ri.Value(), -1.0f, 1.0f);
        }
    }

    return MakeReturnInfo(anyTriggered, value);
}

InputCommand::ReturnInfo InputCommand::EvaluateBinding(const Binding& b) const {
    if (!input_) return MakeReturnInfo(false, 0.0f);

    const auto applyInvertIfNeeded = [&](ReturnInfo ri) {
        if (!b.invertValue) return ri;
        return MakeReturnInfo(ri.Triggered(), -ri.Value());
    };

    switch (b.kind) {
    case DeviceKind::Keyboard: {
        const auto& kb = input_->GetKeyboard();
        const bool down = kb.IsDown(b.key);
        const bool trig = kb.IsTrigger(b.key);
        const bool rel = kb.IsRelease(b.key);
        const bool fired = EvaluateDigital(down, trig, rel, b.state);
        return applyInvertIfNeeded(MakeReturnInfo(fired, down ? 1.0f : 0.0f));
    }
    case DeviceKind::MouseButton: {
        const auto& ms = input_->GetMouse();
        const bool down = ms.IsButtonDown(b.code);
        const bool trig = ms.IsButtonTrigger(b.code);
        const bool rel = ms.IsButtonRelease(b.code);
        const bool fired = EvaluateDigital(down, trig, rel, b.state);
        return applyInvertIfNeeded(MakeReturnInfo(fired, down ? 1.0f : 0.0f));
    }
    case DeviceKind::MouseAxis: {
        const auto& ms = input_->GetMouse();
        float v = 0.0f;

        const HWND hwnd = (b.mouseSpace == MouseSpace::Client) ? static_cast<HWND>(b.hwnd) : nullptr;

        switch (b.mouseAxis) {
        case MouseAxis::X:
            v = static_cast<float>(hwnd ? ms.GetX(hwnd) : ms.GetPos((HWND)nullptr).x);
            break;
        case MouseAxis::Y:
            v = static_cast<float>(hwnd ? ms.GetY(hwnd) : ms.GetPos((HWND)nullptr).y);
            break;
        case MouseAxis::DeltaX:
            v = static_cast<float>(ms.GetDeltaX());
            break;
        case MouseAxis::DeltaY:
            v = static_cast<float>(ms.GetDeltaY());
            break;
        case MouseAxis::Wheel:
            v = static_cast<float>(ms.GetWheelValue());
            break;
        case MouseAxis::DeltaWheel:
            v = static_cast<float>(ms.GetWheel());
            break;
        default:
            v = 0.0f;
            break;
        }

        const bool fired = AxisTriggered(v, b.threshold);
        return applyInvertIfNeeded(MakeReturnInfo(fired, v));
    }
    case DeviceKind::ControllerButton: {
        const auto& ct = input_->GetController();
        const int idx = b.controllerIndex;
        if (!ct.IsConnected(idx)) return MakeReturnInfo(false, 0.0f);
        const bool down = ct.IsButtonDown(b.controllerButton, idx);
        const bool trig = ct.IsButtonTrigger(b.controllerButton, idx);
        const bool rel = ct.IsButtonRelease(b.controllerButton, idx);
        const bool fired = EvaluateDigital(down, trig, rel, b.state);
        return applyInvertIfNeeded(MakeReturnInfo(fired, down ? 1.0f : 0.0f));
    }
    case DeviceKind::ControllerAnalog: {
        const auto& ct = input_->GetController();
        const int idx = b.controllerIndex;
        if (!ct.IsConnected(idx)) return MakeReturnInfo(false, 0.0f);

        float v = 0.0f;
        switch (static_cast<ControllerAnalog>(b.code)) {
        case ControllerAnalog::LeftTrigger: v = NormalizeTrigger255(ct.GetLeftTrigger(idx)); break;
        case ControllerAnalog::RightTrigger: v = NormalizeTrigger255(ct.GetRightTrigger(idx)); break;
        case ControllerAnalog::LeftStickX: v = NormalizeStickInt16(ct.GetLeftStickX(idx)); break;
        case ControllerAnalog::LeftStickY: v = NormalizeStickInt16(ct.GetLeftStickY(idx)); break;
        case ControllerAnalog::RightStickX: v = NormalizeStickInt16(ct.GetRightStickX(idx)); break;
        case ControllerAnalog::RightStickY: v = NormalizeStickInt16(ct.GetRightStickY(idx)); break;
        default: v = 0.0f; break;
        }

        const float t = b.threshold;
        bool fired = false;
        switch (b.state) {
        case InputState::Down:
            fired = std::abs(v) > t;
            break;
        case InputState::Trigger:
            fired = std::abs(v) > t;
            break;
        case InputState::Release:
            fired = std::abs(v) <= t;
            break;
        default:
            fired = false;
            break;
        }

        return applyInvertIfNeeded(MakeReturnInfo(fired, v));
    }
    case DeviceKind::ControllerAnalogDelta: {
        const auto& ct = input_->GetController();
        const int idx = b.controllerIndex;
        if (!ct.IsConnected(idx)) return MakeReturnInfo(false, 0.0f);

        float dv = 0.0f;
        switch (static_cast<ControllerAnalog>(b.code)) {
        case ControllerAnalog::LeftTrigger: dv = NormalizeTriggerDelta255(ct.GetDeltaLeftTrigger(idx)); break;
        case ControllerAnalog::RightTrigger: dv = NormalizeTriggerDelta255(ct.GetDeltaRightTrigger(idx)); break;
        case ControllerAnalog::LeftStickX: dv = NormalizeStickDeltaInt16(ct.GetDeltaLeftStickX(idx)); break;
        case ControllerAnalog::LeftStickY: dv = NormalizeStickDeltaInt16(ct.GetDeltaLeftStickY(idx)); break;
        case ControllerAnalog::RightStickX: dv = NormalizeStickDeltaInt16(ct.GetDeltaRightStickX(idx)); break;
        case ControllerAnalog::RightStickY: dv = NormalizeStickDeltaInt16(ct.GetDeltaRightStickY(idx)); break;
        default: dv = 0.0f; break;
        }

        const bool fired = AxisTriggered(dv, b.threshold);
        return applyInvertIfNeeded(MakeReturnInfo(fired, dv));
    }
    default:
        return MakeReturnInfo(false, 0.0f);
    }
}

} // namespace KashipanEngine
