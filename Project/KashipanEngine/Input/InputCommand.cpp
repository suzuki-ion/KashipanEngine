#include "Input/InputCommand.h"

#include "Input/Input.h"
#include "Input/Keyboard.h"
#include "Input/Mouse.h"
#include "Input/Controller.h"
#include "Utilities/FileIO/JSON.h"

#include <algorithm>
#include <cmath>

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

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

void InputCommand::RegisterCommand(const std::string& action, Key key, InputState state, bool invertValue) {
    if (action.empty()) return;
    Binding b{};
    b.kind = DeviceKind::Keyboard;
    b.state = state;
    b.key = key;
    b.invertValue = invertValue;
    bindings_[action].push_back(b);
}

void InputCommand::RegisterCommand(const std::string& action, MouseButton button, InputState state, bool invertValue) {
    if (action.empty()) return;
    Binding b{};
    b.kind = DeviceKind::MouseButton;
    b.state = state;
    b.code = static_cast<int>(button);
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

#if defined(USE_IMGUI)
void InputCommand::ShowImGui() {
    if (!ImGui::Begin("InputCommand - Registered Commands")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Command Count: %d", static_cast<int>(bindings_.size()));

    if (bindings_.empty()) {
        ImGui::TextUnformatted("No registered input commands.");
        ImGui::End();
        return;
    }

    for (const auto& [action, binds] : bindings_) {
        const ReturnInfo cmdResult = Evaluate(action);

        ImGui::PushID(action.c_str());
        if (ImGui::TreeNode("%s", action.c_str())) {
            ImGui::Text("Result: Triggered=%s  Value=%.3f", cmdResult.Triggered() ? "true" : "false", cmdResult.Value());
            ImGui::Text("Binding Count: %d", static_cast<int>(binds.size()));

            for (size_t i = 0; i < binds.size(); ++i) {
                const auto& b = binds[i];
                const ReturnInfo bindResult = EvaluateBinding(b);

                ImGui::PushID(static_cast<int>(i));
                if (ImGui::TreeNode("Binding")) {
                    ImGui::Text("Index: %d", static_cast<int>(i));
                    ImGui::Text("Device: %s", DeviceKindToString(b.kind).c_str());
                    ImGui::Text("Result: Triggered=%s  Value=%.3f", bindResult.Triggered() ? "true" : "false", bindResult.Value());
                    ImGui::Text("InvertValue: %s", b.invertValue ? "true" : "false");

                    switch (b.kind) {
                    case DeviceKind::Keyboard:
                        ImGui::Text("Key: %s", KeyToString(b.key).c_str());
                        ImGui::Text("State: %s", InputStateToString(b.state).c_str());
                        break;
                    case DeviceKind::MouseButton:
                        ImGui::Text("Button: %s", MouseButtonToString(static_cast<MouseButton>(b.code)).c_str());
                        ImGui::Text("State: %s", InputStateToString(b.state).c_str());
                        break;
                    case DeviceKind::MouseAxis:
                        ImGui::Text("Axis: %s", MouseAxisToString(b.mouseAxis).c_str());
                        ImGui::Text("Threshold: %.3f", b.threshold);
                        ImGui::Text("Space: %s", b.mouseSpace == MouseSpace::Client ? "Client" : "Screen");
                        break;
                    case DeviceKind::ControllerButton:
                        ImGui::Text("Button: %s", ControllerButtonToString(b.controllerButton).c_str());
                        ImGui::Text("State: %s", InputStateToString(b.state).c_str());
                        ImGui::Text("ControllerIndex: %d", b.controllerIndex);
                        break;
                    case DeviceKind::ControllerAnalog:
                        ImGui::Text("Analog: %s", ControllerAnalogToString(static_cast<ControllerAnalog>(b.code)).c_str());
                        ImGui::Text("State: %s", InputStateToString(b.state).c_str());
                        ImGui::Text("ControllerIndex: %d", b.controllerIndex);
                        ImGui::Text("Threshold: %.3f", b.threshold);
                        break;
                    case DeviceKind::ControllerAnalogDelta:
                        ImGui::Text("Analog: %s", ControllerAnalogToString(static_cast<ControllerAnalog>(b.code)).c_str());
                        ImGui::Text("ControllerIndex: %d", b.controllerIndex);
                        ImGui::Text("Threshold: %.3f", b.threshold);
                        break;
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    ImGui::End();
}
#endif

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

        // Get delta (previous frame difference) and compute previous value
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

        const float prev = v - dv;

        if (t >= 0.0f) {
            // Positive-direction threshold
            switch (b.state) {
            case InputState::Down:
                fired = (v > t);
                break;
            case InputState::Trigger:
                fired = (prev <= t) && (v > t);
                break;
            case InputState::Release:
                fired = (prev > t) && (v <= t);
                break;
            default:
                fired = false;
                break;
            }
        } else {
            // Negative-direction threshold
            switch (b.state) {
            case InputState::Down:
                fired = (v < t);
                break;
            case InputState::Trigger:
                fired = (prev >= t) && (v < t);
                break;
            case InputState::Release:
                fired = (prev < t) && (v >= t);
                break;
            default:
                fired = false;
                break;
            }
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

// ===== JSON import/export =====

bool InputCommand::SaveToJSON(const std::string& filepath) const {
    JSON root = JSON::object();

    for (const auto& [action, binds] : bindings_) {
        JSON arr = JSON::array();
        for (const auto& b : binds) {
            JSON entry = JSON::object();
            entry["device"] = DeviceKindToString(b.kind);
            entry["invertValue"] = b.invertValue;

            switch (b.kind) {
            case DeviceKind::Keyboard:
                entry["key"] = KeyToString(b.key);
                entry["state"] = InputStateToString(b.state);
                break;
            case DeviceKind::MouseButton:
                entry["button"] = MouseButtonToString(static_cast<MouseButton>(b.code));
                entry["state"] = InputStateToString(b.state);
                break;
            case DeviceKind::MouseAxis:
                entry["axis"] = MouseAxisToString(b.mouseAxis);
                entry["threshold"] = b.threshold;
                break;
            case DeviceKind::ControllerButton:
                entry["button"] = ControllerButtonToString(b.controllerButton);
                entry["state"] = InputStateToString(b.state);
                entry["controllerIndex"] = b.controllerIndex;
                break;
            case DeviceKind::ControllerAnalog:
                entry["analog"] = ControllerAnalogToString(static_cast<ControllerAnalog>(b.code));
                entry["state"] = InputStateToString(b.state);
                entry["controllerIndex"] = b.controllerIndex;
                entry["threshold"] = b.threshold;
                break;
            case DeviceKind::ControllerAnalogDelta:
                entry["analog"] = ControllerAnalogToString(static_cast<ControllerAnalog>(b.code));
                entry["controllerIndex"] = b.controllerIndex;
                entry["threshold"] = b.threshold;
                break;
            }

            arr.push_back(std::move(entry));
        }
        root[action] = std::move(arr);
    }

    return SaveJSON(root, filepath);
}

bool InputCommand::LoadFromJSON(const std::string& filepath) {
    if (!IsJSONFileValid(filepath)) return false;

    JSON root = LoadJSON(filepath);
    if (root.is_null() || !root.is_object()) return false;

    bindings_.clear();

    for (auto it = root.begin(); it != root.end(); ++it) {
        const std::string& action = it.key();
        if (!it.value().is_array()) continue;

        for (const auto& entry : it.value()) {
            if (!entry.is_object() || !entry.contains("device")) continue;

            const DeviceKind kind = StringToDeviceKind(entry.value("device", ""));
            const bool invertValue = entry.value("invertValue", false);

            switch (kind) {
            case DeviceKind::Keyboard: {
                Key key = StringToKey(entry.value("key", ""));
                InputState state = StringToInputState(entry.value("state", ""));
                Binding b{};
                b.kind = DeviceKind::Keyboard;
                b.key = key;
                b.state = state;
                b.invertValue = invertValue;
                bindings_[action].push_back(b);
                break;
            }
            case DeviceKind::MouseButton: {
                MouseButton btn = StringToMouseButton(entry.value("button", ""));
                InputState state = StringToInputState(entry.value("state", ""));
                Binding b{};
                b.kind = DeviceKind::MouseButton;
                b.code = static_cast<int>(btn);
                b.state = state;
                b.invertValue = invertValue;
                bindings_[action].push_back(b);
                break;
            }
            case DeviceKind::MouseAxis: {
                MouseAxis axis = StringToMouseAxis(entry.value("axis", ""));
                float threshold = entry.value("threshold", 0.0f);
                Binding b{};
                b.kind = DeviceKind::MouseAxis;
                b.mouseAxis = axis;
                b.mouseSpace = MouseSpace::Screen; // JSON では hwnd を保持できないため Screen 固定
                b.threshold = threshold;
                b.invertValue = invertValue;
                bindings_[action].push_back(b);
                break;
            }
            case DeviceKind::ControllerButton: {
                ControllerButton btn = StringToControllerButton(entry.value("button", ""));
                InputState state = StringToInputState(entry.value("state", ""));
                int idx = entry.value("controllerIndex", 0);
                Binding b{};
                b.kind = DeviceKind::ControllerButton;
                b.controllerButton = btn;
                b.state = state;
                b.controllerIndex = idx;
                b.invertValue = invertValue;
                bindings_[action].push_back(b);
                break;
            }
            case DeviceKind::ControllerAnalog: {
                ControllerAnalog analog = StringToControllerAnalog(entry.value("analog", ""));
                InputState state = StringToInputState(entry.value("state", ""));
                int idx = entry.value("controllerIndex", 0);
                float threshold = entry.value("threshold", 0.0f);
                Binding b{};
                b.kind = DeviceKind::ControllerAnalog;
                b.code = static_cast<int>(analog);
                b.state = state;
                b.controllerIndex = idx;
                b.threshold = threshold;
                b.invertValue = invertValue;
                bindings_[action].push_back(b);
                break;
            }
            case DeviceKind::ControllerAnalogDelta: {
                ControllerAnalog analog = StringToControllerAnalog(entry.value("analog", ""));
                int idx = entry.value("controllerIndex", 0);
                float threshold = entry.value("threshold", 0.0f);
                Binding b{};
                b.kind = DeviceKind::ControllerAnalogDelta;
                b.code = static_cast<int>(analog);
                b.controllerIndex = idx;
                b.threshold = threshold;
                b.invertValue = invertValue;
                bindings_[action].push_back(b);
                break;
            }
            default:
                break;
            }
        }
    }

    return true;
}

// ===== String conversion utilities =====

std::string InputCommand::KeyToString(Key key) {
    static const std::unordered_map<Key, std::string> kMap = {
        {Key::Unknown, "Unknown"},
        {Key::A, "A"}, {Key::B, "B"}, {Key::C, "C"}, {Key::D, "D"},
        {Key::E, "E"}, {Key::F, "F"}, {Key::G, "G"}, {Key::H, "H"},
        {Key::I, "I"}, {Key::J, "J"}, {Key::K, "K"}, {Key::L, "L"},
        {Key::M, "M"}, {Key::N, "N"}, {Key::O, "O"}, {Key::P, "P"},
        {Key::Q, "Q"}, {Key::R, "R"}, {Key::S, "S"}, {Key::T, "T"},
        {Key::U, "U"}, {Key::V, "V"}, {Key::W, "W"}, {Key::X, "X"},
        {Key::Y, "Y"}, {Key::Z, "Z"},
        {Key::D0, "D0"}, {Key::D1, "D1"}, {Key::D2, "D2"}, {Key::D3, "D3"},
        {Key::D4, "D4"}, {Key::D5, "D5"}, {Key::D6, "D6"}, {Key::D7, "D7"},
        {Key::D8, "D8"}, {Key::D9, "D9"},
        {Key::Left, "Left"}, {Key::Right, "Right"}, {Key::Up, "Up"}, {Key::Down, "Down"},
        {Key::LeftShift, "LeftShift"}, {Key::RightShift, "RightShift"},
        {Key::LeftControl, "LeftControl"}, {Key::RightControl, "RightControl"},
        {Key::LeftAlt, "LeftAlt"}, {Key::RightAlt, "RightAlt"},
        {Key::Shift, "Shift"}, {Key::Control, "Control"}, {Key::Alt, "Alt"},
        {Key::Space, "Space"}, {Key::Enter, "Enter"}, {Key::Escape, "Escape"},
        {Key::Tab, "Tab"}, {Key::Backspace, "Backspace"},
        {Key::F1, "F1"}, {Key::F2, "F2"}, {Key::F3, "F3"}, {Key::F4, "F4"},
        {Key::F5, "F5"}, {Key::F6, "F6"}, {Key::F7, "F7"}, {Key::F8, "F8"},
        {Key::F9, "F9"}, {Key::F10, "F10"}, {Key::F11, "F11"}, {Key::F12, "F12"},
    };
    auto it = kMap.find(key);
    return (it != kMap.end()) ? it->second : "Unknown";
}

Key InputCommand::StringToKey(const std::string& str) {
    static const std::unordered_map<std::string, Key> kMap = {
        {"Unknown", Key::Unknown},
        {"A", Key::A}, {"B", Key::B}, {"C", Key::C}, {"D", Key::D},
        {"E", Key::E}, {"F", Key::F}, {"G", Key::G}, {"H", Key::H},
        {"I", Key::I}, {"J", Key::J}, {"K", Key::K}, {"L", Key::L},
        {"M", Key::M}, {"N", Key::N}, {"O", Key::O}, {"P", Key::P},
        {"Q", Key::Q}, {"R", Key::R}, {"S", Key::S}, {"T", Key::T},
        {"U", Key::U}, {"V", Key::V}, {"W", Key::W}, {"X", Key::X},
        {"Y", Key::Y}, {"Z", Key::Z},
        {"D0", Key::D0}, {"D1", Key::D1}, {"D2", Key::D2}, {"D3", Key::D3},
        {"D4", Key::D4}, {"D5", Key::D5}, {"D6", Key::D6}, {"D7", Key::D7},
        {"D8", Key::D8}, {"D9", Key::D9},
        {"Left", Key::Left}, {"Right", Key::Right}, {"Up", Key::Up}, {"Down", Key::Down},
        {"LeftShift", Key::LeftShift}, {"RightShift", Key::RightShift},
        {"LeftControl", Key::LeftControl}, {"RightControl", Key::RightControl},
        {"LeftAlt", Key::LeftAlt}, {"RightAlt", Key::RightAlt},
        {"Shift", Key::Shift}, {"Control", Key::Control}, {"Alt", Key::Alt},
        {"Space", Key::Space}, {"Enter", Key::Enter}, {"Escape", Key::Escape},
        {"Tab", Key::Tab}, {"Backspace", Key::Backspace},
        {"F1", Key::F1}, {"F2", Key::F2}, {"F3", Key::F3}, {"F4", Key::F4},
        {"F5", Key::F5}, {"F6", Key::F6}, {"F7", Key::F7}, {"F8", Key::F8},
        {"F9", Key::F9}, {"F10", Key::F10}, {"F11", Key::F11}, {"F12", Key::F12},
    };
    auto it = kMap.find(str);
    return (it != kMap.end()) ? it->second : Key::Unknown;
}

std::string InputCommand::MouseButtonToString(MouseButton button) {
    static const std::unordered_map<MouseButton, std::string> kMap = {
        {MouseButton::Left, "Left"}, {MouseButton::Right, "Right"}, {MouseButton::Middle, "Middle"},
        {MouseButton::Button4, "Button4"}, {MouseButton::Button5, "Button5"},
        {MouseButton::Button6, "Button6"}, {MouseButton::Button7, "Button7"}, {MouseButton::Button8, "Button8"},
    };
    auto it = kMap.find(button);
    return (it != kMap.end()) ? it->second : "Left";
}

MouseButton InputCommand::StringToMouseButton(const std::string& str) {
    static const std::unordered_map<std::string, MouseButton> kMap = {
        {"Left", MouseButton::Left}, {"Right", MouseButton::Right}, {"Middle", MouseButton::Middle},
        {"Button4", MouseButton::Button4}, {"Button5", MouseButton::Button5},
        {"Button6", MouseButton::Button6}, {"Button7", MouseButton::Button7}, {"Button8", MouseButton::Button8},
    };
    auto it = kMap.find(str);
    return (it != kMap.end()) ? it->second : MouseButton::Left;
}

std::string InputCommand::ControllerButtonToString(ControllerButton button) {
    static const std::unordered_map<ControllerButton, std::string> kMap = {
        {ControllerButton::DPadUp, "DPadUp"}, {ControllerButton::DPadDown, "DPadDown"},
        {ControllerButton::DPadLeft, "DPadLeft"}, {ControllerButton::DPadRight, "DPadRight"},
        {ControllerButton::Start, "Start"}, {ControllerButton::Back, "Back"},
        {ControllerButton::LeftThumb, "LeftThumb"}, {ControllerButton::RightThumb, "RightThumb"},
        {ControllerButton::LeftShoulder, "LeftShoulder"}, {ControllerButton::RightShoulder, "RightShoulder"},
        {ControllerButton::A, "A"}, {ControllerButton::B, "B"},
        {ControllerButton::X, "X"}, {ControllerButton::Y, "Y"},
    };
    auto it = kMap.find(button);
    return (it != kMap.end()) ? it->second : "A";
}

ControllerButton InputCommand::StringToControllerButton(const std::string& str) {
    static const std::unordered_map<std::string, ControllerButton> kMap = {
        {"DPadUp", ControllerButton::DPadUp}, {"DPadDown", ControllerButton::DPadDown},
        {"DPadLeft", ControllerButton::DPadLeft}, {"DPadRight", ControllerButton::DPadRight},
        {"Start", ControllerButton::Start}, {"Back", ControllerButton::Back},
        {"LeftThumb", ControllerButton::LeftThumb}, {"RightThumb", ControllerButton::RightThumb},
        {"LeftShoulder", ControllerButton::LeftShoulder}, {"RightShoulder", ControllerButton::RightShoulder},
        {"A", ControllerButton::A}, {"B", ControllerButton::B},
        {"X", ControllerButton::X}, {"Y", ControllerButton::Y},
    };
    auto it = kMap.find(str);
    return (it != kMap.end()) ? it->second : ControllerButton::A;
}

std::string InputCommand::ControllerAnalogToString(ControllerAnalog analog) {
    switch (analog) {
    case ControllerAnalog::LeftTrigger:  return "LeftTrigger";
    case ControllerAnalog::RightTrigger: return "RightTrigger";
    case ControllerAnalog::LeftStickX:   return "LeftStickX";
    case ControllerAnalog::LeftStickY:   return "LeftStickY";
    case ControllerAnalog::RightStickX:  return "RightStickX";
    case ControllerAnalog::RightStickY:  return "RightStickY";
    default: return "LeftTrigger";
    }
}

InputCommand::ControllerAnalog InputCommand::StringToControllerAnalog(const std::string& str) {
    static const std::unordered_map<std::string, ControllerAnalog> kMap = {
        {"LeftTrigger", ControllerAnalog::LeftTrigger}, {"RightTrigger", ControllerAnalog::RightTrigger},
        {"LeftStickX", ControllerAnalog::LeftStickX}, {"LeftStickY", ControllerAnalog::LeftStickY},
        {"RightStickX", ControllerAnalog::RightStickX}, {"RightStickY", ControllerAnalog::RightStickY},
    };
    auto it = kMap.find(str);
    return (it != kMap.end()) ? it->second : ControllerAnalog::LeftTrigger;
}

std::string InputCommand::InputStateToString(InputState state) {
    switch (state) {
    case InputState::Down:    return "Down";
    case InputState::Trigger: return "Trigger";
    case InputState::Release: return "Release";
    default: return "Down";
    }
}

InputCommand::InputState InputCommand::StringToInputState(const std::string& str) {
    if (str == "Trigger") return InputState::Trigger;
    if (str == "Release") return InputState::Release;
    return InputState::Down;
}

std::string InputCommand::MouseAxisToString(MouseAxis axis) {
    switch (axis) {
    case MouseAxis::X:          return "X";
    case MouseAxis::Y:          return "Y";
    case MouseAxis::DeltaX:     return "DeltaX";
    case MouseAxis::DeltaY:     return "DeltaY";
    case MouseAxis::Wheel:      return "Wheel";
    case MouseAxis::DeltaWheel: return "DeltaWheel";
    default: return "X";
    }
}

InputCommand::MouseAxis InputCommand::StringToMouseAxis(const std::string& str) {
    static const std::unordered_map<std::string, MouseAxis> kMap = {
        {"X", MouseAxis::X}, {"Y", MouseAxis::Y},
        {"DeltaX", MouseAxis::DeltaX}, {"DeltaY", MouseAxis::DeltaY},
        {"Wheel", MouseAxis::Wheel}, {"DeltaWheel", MouseAxis::DeltaWheel},
    };
    auto it = kMap.find(str);
    return (it != kMap.end()) ? it->second : MouseAxis::X;
}

std::string InputCommand::DeviceKindToString(DeviceKind kind) {
    switch (kind) {
    case DeviceKind::Keyboard:             return "Keyboard";
    case DeviceKind::MouseButton:          return "MouseButton";
    case DeviceKind::MouseAxis:            return "MouseAxis";
    case DeviceKind::ControllerButton:     return "ControllerButton";
    case DeviceKind::ControllerAnalog:     return "ControllerAnalog";
    case DeviceKind::ControllerAnalogDelta:return "ControllerAnalogDelta";
    default: return "Keyboard";
    }
}

InputCommand::DeviceKind InputCommand::StringToDeviceKind(const std::string& str) {
    static const std::unordered_map<std::string, DeviceKind> kMap = {
        {"Keyboard", DeviceKind::Keyboard},
        {"MouseButton", DeviceKind::MouseButton},
        {"MouseAxis", DeviceKind::MouseAxis},
        {"ControllerButton", DeviceKind::ControllerButton},
        {"ControllerAnalog", DeviceKind::ControllerAnalog},
        {"ControllerAnalogDelta", DeviceKind::ControllerAnalogDelta},
    };
    auto it = kMap.find(str);
    return (it != kMap.end()) ? it->second : DeviceKind::Keyboard;
}

} // namespace KashipanEngine
