#include "Input/InputCommand.h"

#include "Debug/Logger.h"

#include "Input/Input.h"
#include "Input/Keyboard.h"
#include "Input/Mouse.h"
#include "Input/Controller.h"
#include "Core/ProjectPaths.h"
#include "Utilities/FileIO/JSON.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

#if defined(USE_IMGUI)
#include <imgui.h>
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

namespace {
float Clamp01(float v) {
    LogScope scope;
    return std::clamp(v, 0.0f, 1.0f);
}

float Clamp11(float v) {
    LogScope scope;
    return std::clamp(v, -1.0f, 1.0f);
}

bool EvaluateDigital(bool down, bool trigger, bool release, InputCommand::InputState state) {
    LogScope scope;
    switch (state) {
    case InputCommand::InputState::Down: return down;
    case InputCommand::InputState::Trigger: return trigger;
    case InputCommand::InputState::Release: return release;
    default: return false;
    }
}

float NormalizeTrigger255(int v) {
    LogScope scope;
    return Clamp01(static_cast<float>(v) / 255.0f);
}

float NormalizeStickInt16(int v) {
    LogScope scope;
    if (v >= 0) return Clamp11(static_cast<float>(v) / 32767.0f);
    return Clamp11(static_cast<float>(v) / 32768.0f);
}

float NormalizeTriggerDelta255(int dv) {
    LogScope scope;
    return Clamp11(static_cast<float>(dv) / 255.0f);
}

float NormalizeStickDeltaInt16(int dv) {
    LogScope scope;
    return Clamp11(static_cast<float>(dv) / 32767.0f);
}

static bool AxisTriggered(float v, float threshold) {
    LogScope scope;
    return std::abs(v) > threshold;
}

#if defined(USE_IMGUI)
// ShowImGui() の追加/編集フォームで使う選択肢一覧
constexpr const char* kDeviceKindNames[] = { "Keyboard", "MouseButton", "MouseAxis", "ControllerButton", "ControllerAnalog", "ControllerAnalogDelta" };
constexpr const char* kInputStateNames[] = { "Down", "Trigger", "Release" };
constexpr const char* kKeyNames[] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9",
    "Left", "Right", "Up", "Down",
    "LeftShift", "RightShift", "LeftControl", "RightControl", "LeftAlt", "RightAlt",
    "Shift", "Control", "Alt",
    "Space", "Enter", "Escape", "Tab", "Backspace",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
};
constexpr const char* kMouseButtonNames[] = { "Left", "Right", "Middle", "Button4", "Button5", "Button6", "Button7", "Button8" };
constexpr const char* kMouseAxisNames[] = { "X", "Y", "DeltaX", "DeltaY", "Wheel", "DeltaWheel" };
constexpr const char* kControllerButtonNames[] = {
    "DPadUp", "DPadDown", "DPadLeft", "DPadRight", "Start", "Back",
    "LeftThumb", "RightThumb", "LeftShoulder", "RightShoulder", "A", "B", "X", "Y",
};
constexpr const char* kControllerAnalogNames[] = { "LeftTrigger", "RightTrigger", "LeftStickX", "LeftStickY", "RightStickX", "RightStickY" };

/// @brief 文字列配列から一致するインデックスを探す（見つからない場合は0）
int FindIndex(const char* const* names, int count, const std::string& value) {
    LogScope scope;
    for (int i = 0; i < count; ++i) {
        if (value == names[i]) return i;
    }
    return 0;
}
#endif
} // namespace

InputCommand::InputCommand(Passkey<GameEngine>, const Input* input) : input_(input) {}

void InputCommand::Clear() {
    LogScope scope;
    bindings_.clear();
}

void InputCommand::RegisterCommand(const std::string& action, Key key, InputState state, bool invertValue) {
    LogScope scope;
    if (action.empty()) return;
    Binding b{};
    b.kind = DeviceKind::Keyboard;
    b.state = state;
    b.key = key;
    b.invertValue = invertValue;
    bindings_[action].push_back(b);
}

void InputCommand::RegisterCommand(const std::string& action, MouseButton button, InputState state, bool invertValue) {
    LogScope scope;
    if (action.empty()) return;
    Binding b{};
    b.kind = DeviceKind::MouseButton;
    b.state = state;
    b.code = static_cast<int>(button);
    b.invertValue = invertValue;
    bindings_[action].push_back(b);
}

void InputCommand::RegisterCommand(const std::string& action, MouseAxis axis, void* hwnd, float threshold, bool invertValue) {
    LogScope scope;
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
    LogScope scope;
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
    LogScope scope;
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
    LogScope scope;
    if (action.empty()) return;
    Binding b{};
    b.kind = DeviceKind::ControllerAnalogDelta;
    b.code = static_cast<int>(analog);
    b.controllerIndex = controllerIndex;
    b.threshold = threshold;
    b.invertValue = invertValue;
    bindings_[action].push_back(b);
}

std::vector<std::string> InputCommand::GetRegisteredCommandNames() const {
    LogScope scope;
    std::vector<std::string> names;
    names.reserve(bindings_.size());
    for (const auto& [action, binds] : bindings_) {
        names.push_back(action);
    }
    std::sort(names.begin(), names.end());
    return names;
}

InputCommand::ReturnInfo InputCommand::Evaluate(const std::string& action) const {
    LogScope scope;
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
    LogScope scope;
    if (!ImGui::Begin(TranslationLabel("editor.inputcommand.window"))) {
        ImGui::End();
        return;
    }

    //--------- 新規バインドの追加 ---------//
    if (ImGui::CollapsingHeader(TranslationLabel("editor.inputcommand.new_binding"), ImGuiTreeNodeFlags_DefaultOpen)) {
        static char actionNameBuffer[128] = "";
        static int deviceKindIndex = 0;
        static int keyIndex = 0;
        static int mouseButtonIndex = 0;
        static int mouseAxisIndex = 0;
        static int controllerButtonIndex = 0;
        static int controllerAnalogIndex = 0;
        static int stateIndex = 0;
        static int controllerIndexValue = 0;
        static float threshold = 0.0f;
        static bool invertValue = false;
        static bool useAnalogDelta = false;

        // 既存のコマンドへ簡単に追加できるよう、登録済みのアクション名を選択肢として提示する
        // （選択すると下のテキスト欄に反映される。新規コマンド名はそのままテキスト欄に入力する）
        {
            std::vector<std::string> existingActions;
            existingActions.reserve(bindings_.size());
            for (const auto& [existingAction, existingBinds] : bindings_) {
                existingActions.push_back(existingAction);
            }
            std::sort(existingActions.begin(), existingActions.end());

            const std::string preview = (actionNameBuffer[0] != '\0') ? actionNameBuffer : "(New Command)";
            if (ImGui::BeginCombo(TranslationLabel("editor.inputcommand.existing_command"), preview.c_str())) {
                if (ImGui::Selectable(TranslationLabel("editor.inputcommand.new_command"), actionNameBuffer[0] == '\0')) {
                    actionNameBuffer[0] = '\0';
                }
                for (const auto& existingAction : existingActions) {
                    const bool selected = (existingAction == actionNameBuffer);
                    if (ImGui::Selectable(existingAction.c_str(), selected)) {
                        std::snprintf(actionNameBuffer, sizeof(actionNameBuffer), "%s", existingAction.c_str());
                    }
                }
                ImGui::EndCombo();
            }
        }
        ImGui::InputText(TranslationLabel("editor.inputcommand.action_name"), actionNameBuffer, sizeof(actionNameBuffer));
        ImGui::Combo(TranslationLabel("editor.inputcommand.device"), &deviceKindIndex, kDeviceKindNames, IM_ARRAYSIZE(kDeviceKindNames));
        const DeviceKind kind = static_cast<DeviceKind>(deviceKindIndex);

        switch (kind) {
        case DeviceKind::Keyboard:
            ImGui::Combo(TranslationLabel("editor.inputcommand.key"), &keyIndex, kKeyNames, IM_ARRAYSIZE(kKeyNames));
            ImGui::Combo(TranslationLabel("editor.inputcommand.state"), &stateIndex, kInputStateNames, IM_ARRAYSIZE(kInputStateNames));
            break;
        case DeviceKind::MouseButton:
            ImGui::Combo(TranslationLabel("editor.inputcommand.button"), &mouseButtonIndex, kMouseButtonNames, IM_ARRAYSIZE(kMouseButtonNames));
            ImGui::Combo(TranslationLabel("editor.inputcommand.state"), &stateIndex, kInputStateNames, IM_ARRAYSIZE(kInputStateNames));
            break;
        case DeviceKind::MouseAxis:
            ImGui::Combo(TranslationLabel("editor.inputcommand.axis"), &mouseAxisIndex, kMouseAxisNames, IM_ARRAYSIZE(kMouseAxisNames));
            ImGui::DragFloat(TranslationLabel("editor.inputcommand.threshold"), &threshold, 0.01f);
            break;
        case DeviceKind::ControllerButton:
            ImGui::Combo(TranslationLabel("editor.inputcommand.button"), &controllerButtonIndex, kControllerButtonNames, IM_ARRAYSIZE(kControllerButtonNames));
            ImGui::Combo(TranslationLabel("editor.inputcommand.state"), &stateIndex, kInputStateNames, IM_ARRAYSIZE(kInputStateNames));
            ImGui::InputInt(TranslationLabel("editor.inputcommand.controller_index"), &controllerIndexValue);
            break;
        case DeviceKind::ControllerAnalog:
        case DeviceKind::ControllerAnalogDelta:
            ImGui::Combo(TranslationLabel("editor.inputcommand.analog"), &controllerAnalogIndex, kControllerAnalogNames, IM_ARRAYSIZE(kControllerAnalogNames));
            ImGui::Checkbox(TranslationLabel("editor.inputcommand.delta_mode"), &useAnalogDelta);
            if (!useAnalogDelta) {
                ImGui::Combo(TranslationLabel("editor.inputcommand.state"), &stateIndex, kInputStateNames, IM_ARRAYSIZE(kInputStateNames));
            }
            ImGui::InputInt(TranslationLabel("editor.inputcommand.controller_index"), &controllerIndexValue);
            ImGui::DragFloat(TranslationLabel("editor.inputcommand.threshold"), &threshold, 0.01f);
            break;
        }
        ImGui::Checkbox(TranslationLabel("editor.inputcommand.invert_value"), &invertValue);

        ImGui::BeginDisabled(actionNameBuffer[0] == '\0');
        if (ImGui::Button(TranslationLabel("editor.inputcommand.add_binding"))) {
            const std::string action = actionNameBuffer;
            const InputState state = static_cast<InputState>(stateIndex);
            switch (kind) {
            case DeviceKind::Keyboard:
                RegisterCommand(action, StringToKey(kKeyNames[keyIndex]), state, invertValue);
                break;
            case DeviceKind::MouseButton:
                RegisterCommand(action, StringToMouseButton(kMouseButtonNames[mouseButtonIndex]), state, invertValue);
                break;
            case DeviceKind::MouseAxis:
                RegisterCommand(action, StringToMouseAxis(kMouseAxisNames[mouseAxisIndex]), nullptr, threshold, invertValue);
                break;
            case DeviceKind::ControllerButton:
                RegisterCommand(action, StringToControllerButton(kControllerButtonNames[controllerButtonIndex]), state, controllerIndexValue, invertValue);
                break;
            case DeviceKind::ControllerAnalog:
                if (useAnalogDelta) {
                    RegisterCommand(action, StringToControllerAnalog(kControllerAnalogNames[controllerAnalogIndex]), controllerIndexValue, threshold, invertValue);
                } else {
                    RegisterCommand(action, StringToControllerAnalog(kControllerAnalogNames[controllerAnalogIndex]), state, controllerIndexValue, threshold, invertValue);
                }
                break;
            default:
                break;
            }
        }
        ImGui::EndDisabled();
    }

    ImGui::Separator();
    ImGui::Text(TranslationC("editor.inputcommand.command_count_d"), static_cast<int>(bindings_.size()));

    //--------- 登録済みコマンドの一覧・編集・削除 ---------//
    // ループ中に bindings_ を直接変更するとイテレータが壊れるため、削除・リネーム・複製要求はループ後にまとめて適用する
    std::string pendingRemoveAction;
    std::vector<std::pair<std::string, size_t>> pendingRemoveBindings;
    static std::string renamingAction;
    static char renameBuffer[128] = "";
    std::string pendingRenameFrom;
    std::string pendingRenameTo;
    std::string pendingDuplicateAction;
    // OpenPopup/BeginPopupはIDスタックの深さが一致していないと同一ポップアップとして扱われないため、
    // ループ内(PushID(action)の中)ではOpenPopupを呼ばず、ループ外でまとめて呼び出す
    bool requestOpenRenamePopup = false;

    for (auto& [action, binds] : bindings_) {
        const ReturnInfo cmdResult = Evaluate(action);

        ImGui::PushID(action.c_str());
        const bool open = ImGui::TreeNode("%s", action.c_str());
        if (ImGui::BeginPopupContextItem("CommandContextMenu")) {
            if (ImGui::MenuItem(TranslationLabel("editor.inputcommand.rename"))) {
                renamingAction = action;
                std::snprintf(renameBuffer, sizeof(renameBuffer), "%s", action.c_str());
                requestOpenRenamePopup = true;
            }
            if (ImGui::MenuItem(TranslationLabel("editor.inputcommand.duplicate"))) {
                pendingDuplicateAction = action;
            }
            if (ImGui::MenuItem(TranslationLabel("editor.inputcommand.delete_command"))) {
                pendingRemoveAction = action;
            }
            ImGui::EndPopup();
        }
        if (open) {
            ImGui::Text(TranslationC("editor.inputcommand.result_triggered_s_value_3f"), cmdResult.Triggered() ? "true" : "false", cmdResult.Value());
            ImGui::Text(TranslationC("editor.inputcommand.binding_count_d"), static_cast<int>(binds.size()));

            for (size_t i = 0; i < binds.size(); ++i) {
                auto& b = binds[i];
                const ReturnInfo bindResult = EvaluateBinding(b);

                ImGui::PushID(static_cast<int>(i));
                if (ImGui::TreeNode(TranslationLabel("editor.inputcommand.binding"))) {
                    ImGui::Text(TranslationC("editor.inputcommand.device_s"), DeviceKindToString(b.kind).c_str());
                    ImGui::Text(TranslationC("editor.inputcommand.result_triggered_s_value_3f"), bindResult.Triggered() ? "true" : "false", bindResult.Value());
                    ImGui::Checkbox(TranslationLabel("editor.inputcommand.invert_value"), &b.invertValue);

                    switch (b.kind) {
                    case DeviceKind::Keyboard: {
                        int idx = FindIndex(kKeyNames, IM_ARRAYSIZE(kKeyNames), KeyToString(b.key));
                        if (ImGui::Combo(TranslationLabel("editor.inputcommand.key"), &idx, kKeyNames, IM_ARRAYSIZE(kKeyNames))) b.key = StringToKey(kKeyNames[idx]);
                        int stateIdx = static_cast<int>(b.state);
                        if (ImGui::Combo(TranslationLabel("editor.inputcommand.state"), &stateIdx, kInputStateNames, IM_ARRAYSIZE(kInputStateNames))) b.state = static_cast<InputState>(stateIdx);
                        break;
                    }
                    case DeviceKind::MouseButton: {
                        int idx = FindIndex(kMouseButtonNames, IM_ARRAYSIZE(kMouseButtonNames), MouseButtonToString(static_cast<MouseButton>(b.code)));
                        if (ImGui::Combo(TranslationLabel("editor.inputcommand.button"), &idx, kMouseButtonNames, IM_ARRAYSIZE(kMouseButtonNames))) b.code = static_cast<int>(StringToMouseButton(kMouseButtonNames[idx]));
                        int stateIdx = static_cast<int>(b.state);
                        if (ImGui::Combo(TranslationLabel("editor.inputcommand.state"), &stateIdx, kInputStateNames, IM_ARRAYSIZE(kInputStateNames))) b.state = static_cast<InputState>(stateIdx);
                        break;
                    }
                    case DeviceKind::MouseAxis: {
                        int idx = FindIndex(kMouseAxisNames, IM_ARRAYSIZE(kMouseAxisNames), MouseAxisToString(b.mouseAxis));
                        if (ImGui::Combo(TranslationLabel("editor.inputcommand.axis"), &idx, kMouseAxisNames, IM_ARRAYSIZE(kMouseAxisNames))) b.mouseAxis = StringToMouseAxis(kMouseAxisNames[idx]);
                        ImGui::DragFloat(TranslationLabel("editor.inputcommand.threshold"), &b.threshold, 0.01f);
                        ImGui::Text(TranslationC("editor.inputcommand.space_s"), b.mouseSpace == MouseSpace::Client ? "Client" : "Screen");
                        break;
                    }
                    case DeviceKind::ControllerButton: {
                        int idx = FindIndex(kControllerButtonNames, IM_ARRAYSIZE(kControllerButtonNames), ControllerButtonToString(b.controllerButton));
                        if (ImGui::Combo(TranslationLabel("editor.inputcommand.button"), &idx, kControllerButtonNames, IM_ARRAYSIZE(kControllerButtonNames))) b.controllerButton = StringToControllerButton(kControllerButtonNames[idx]);
                        int stateIdx = static_cast<int>(b.state);
                        if (ImGui::Combo(TranslationLabel("editor.inputcommand.state"), &stateIdx, kInputStateNames, IM_ARRAYSIZE(kInputStateNames))) b.state = static_cast<InputState>(stateIdx);
                        ImGui::InputInt(TranslationLabel("editor.inputcommand.controller_index"), &b.controllerIndex);
                        break;
                    }
                    case DeviceKind::ControllerAnalog: {
                        int idx = FindIndex(kControllerAnalogNames, IM_ARRAYSIZE(kControllerAnalogNames), ControllerAnalogToString(static_cast<ControllerAnalog>(b.code)));
                        if (ImGui::Combo(TranslationLabel("editor.inputcommand.analog"), &idx, kControllerAnalogNames, IM_ARRAYSIZE(kControllerAnalogNames))) b.code = static_cast<int>(StringToControllerAnalog(kControllerAnalogNames[idx]));
                        int stateIdx = static_cast<int>(b.state);
                        if (ImGui::Combo(TranslationLabel("editor.inputcommand.state"), &stateIdx, kInputStateNames, IM_ARRAYSIZE(kInputStateNames))) b.state = static_cast<InputState>(stateIdx);
                        ImGui::InputInt(TranslationLabel("editor.inputcommand.controller_index"), &b.controllerIndex);
                        ImGui::DragFloat(TranslationLabel("editor.inputcommand.threshold"), &b.threshold, 0.01f);
                        break;
                    }
                    case DeviceKind::ControllerAnalogDelta: {
                        int idx = FindIndex(kControllerAnalogNames, IM_ARRAYSIZE(kControllerAnalogNames), ControllerAnalogToString(static_cast<ControllerAnalog>(b.code)));
                        if (ImGui::Combo(TranslationLabel("editor.inputcommand.analog"), &idx, kControllerAnalogNames, IM_ARRAYSIZE(kControllerAnalogNames))) b.code = static_cast<int>(StringToControllerAnalog(kControllerAnalogNames[idx]));
                        ImGui::InputInt(TranslationLabel("editor.inputcommand.controller_index"), &b.controllerIndex);
                        ImGui::DragFloat(TranslationLabel("editor.inputcommand.threshold"), &b.threshold, 0.01f);
                        break;
                    }
                    }

                    if (ImGui::SmallButton(TranslationLabel("editor.inputcommand.remove_binding"))) {
                        pendingRemoveBindings.emplace_back(action, i);
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    // リネームポップアップ本体（OpenPopupと同じIDスタックの深さ、ループの外で呼び出す）
    if (requestOpenRenamePopup) {
        ImGui::OpenPopup("RenameCommandPopup");
    }
    if (ImGui::BeginPopup("RenameCommandPopup")) {
        ImGui::Text(TranslationC("editor.inputcommand.rename_s_to"), renamingAction.c_str());
        ImGui::InputText("##RenameInput", renameBuffer, sizeof(renameBuffer));
        if (ImGui::Button(TranslationLabel("editor.inputcommand.rename_2"))) {
            if (renameBuffer[0] != '\0' && renamingAction != renameBuffer) {
                pendingRenameFrom = renamingAction;
                pendingRenameTo = renameBuffer;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(TranslationLabel("editor.inputcommand.cancel"))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (!pendingRemoveAction.empty()) {
        bindings_.erase(pendingRemoveAction);
    }
    for (const auto& [action, index] : pendingRemoveBindings) {
        auto it = bindings_.find(action);
        if (it != bindings_.end() && index < it->second.size()) {
            it->second.erase(it->second.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }
    if (!pendingRenameFrom.empty() && !pendingRenameTo.empty() && pendingRenameFrom != pendingRenameTo) {
        auto it = bindings_.find(pendingRenameFrom);
        if (it != bindings_.end()) {
            std::vector<Binding> moved = std::move(it->second);
            bindings_.erase(it);
            auto& target = bindings_[pendingRenameTo];
            target.insert(target.end(), moved.begin(), moved.end());
        }
    }
    if (!pendingDuplicateAction.empty()) {
        auto it = bindings_.find(pendingDuplicateAction);
        if (it != bindings_.end()) {
            // 複製元のデータを先にコピーしておく（bindings_[candidate]の挿入でrehashが起き、
            // イテレータitが無効化された後にit->secondへアクセスするのを避けるため）
            std::vector<Binding> copy = it->second;
            // 名前が衝突しないよう "(1)", "(2)", ... と番号を振っていく
            std::string candidate;
            int suffix = 1;
            do {
                candidate = pendingDuplicateAction + " (" + std::to_string(suffix) + ")";
                ++suffix;
            } while (bindings_.find(candidate) != bindings_.end());
            bindings_[candidate] = std::move(copy);
        }
    }

    ImGui::End();
}
#endif

InputCommand::ReturnInfo InputCommand::EvaluateBinding(const Binding& b) const {
    LogScope scope;
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
    LogScope scope;
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

    return SaveJSON(root, ProjectPaths::ToPhysical(filepath));
}

bool InputCommand::LoadFromJSON(const std::string& filepath) {
    LogScope scope;
    const std::string resolvedPath = ProjectPaths::ToPhysical(filepath);
    if (!IsJSONFileValid(resolvedPath)) return false;

    JSON root = LoadJSON(resolvedPath);
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
    LogScope scope;
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
    LogScope scope;
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
    LogScope scope;
    static const std::unordered_map<MouseButton, std::string> kMap = {
        {MouseButton::Left, "Left"}, {MouseButton::Right, "Right"}, {MouseButton::Middle, "Middle"},
        {MouseButton::Button4, "Button4"}, {MouseButton::Button5, "Button5"},
        {MouseButton::Button6, "Button6"}, {MouseButton::Button7, "Button7"}, {MouseButton::Button8, "Button8"},
    };
    auto it = kMap.find(button);
    return (it != kMap.end()) ? it->second : "Left";
}

MouseButton InputCommand::StringToMouseButton(const std::string& str) {
    LogScope scope;
    static const std::unordered_map<std::string, MouseButton> kMap = {
        {"Left", MouseButton::Left}, {"Right", MouseButton::Right}, {"Middle", MouseButton::Middle},
        {"Button4", MouseButton::Button4}, {"Button5", MouseButton::Button5},
        {"Button6", MouseButton::Button6}, {"Button7", MouseButton::Button7}, {"Button8", MouseButton::Button8},
    };
    auto it = kMap.find(str);
    return (it != kMap.end()) ? it->second : MouseButton::Left;
}

std::string InputCommand::ControllerButtonToString(ControllerButton button) {
    LogScope scope;
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
    LogScope scope;
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
    LogScope scope;
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
    LogScope scope;
    static const std::unordered_map<std::string, ControllerAnalog> kMap = {
        {"LeftTrigger", ControllerAnalog::LeftTrigger}, {"RightTrigger", ControllerAnalog::RightTrigger},
        {"LeftStickX", ControllerAnalog::LeftStickX}, {"LeftStickY", ControllerAnalog::LeftStickY},
        {"RightStickX", ControllerAnalog::RightStickX}, {"RightStickY", ControllerAnalog::RightStickY},
    };
    auto it = kMap.find(str);
    return (it != kMap.end()) ? it->second : ControllerAnalog::LeftTrigger;
}

std::string InputCommand::InputStateToString(InputState state) {
    LogScope scope;
    switch (state) {
    case InputState::Down:    return "Down";
    case InputState::Trigger: return "Trigger";
    case InputState::Release: return "Release";
    default: return "Down";
    }
}

InputCommand::InputState InputCommand::StringToInputState(const std::string& str) {
    LogScope scope;
    if (str == "Trigger") return InputState::Trigger;
    if (str == "Release") return InputState::Release;
    return InputState::Down;
}

std::string InputCommand::MouseAxisToString(MouseAxis axis) {
    LogScope scope;
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
    LogScope scope;
    static const std::unordered_map<std::string, MouseAxis> kMap = {
        {"X", MouseAxis::X}, {"Y", MouseAxis::Y},
        {"DeltaX", MouseAxis::DeltaX}, {"DeltaY", MouseAxis::DeltaY},
        {"Wheel", MouseAxis::Wheel}, {"DeltaWheel", MouseAxis::DeltaWheel},
    };
    auto it = kMap.find(str);
    return (it != kMap.end()) ? it->second : MouseAxis::X;
}

std::string InputCommand::DeviceKindToString(DeviceKind kind) {
    LogScope scope;
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
    LogScope scope;
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
