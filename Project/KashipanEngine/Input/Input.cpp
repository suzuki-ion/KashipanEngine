#include "Input/Input.h"

#include "Input/Keyboard.h"
#include "Input/Mouse.h"
#include "Input/Controller.h"
#include "EngineSettings.h"

#include <algorithm>
#include <cmath>

#if defined(USE_IMGUI)
#include <imgui.h>
#include <cctype>
#include <string>
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

Input::Input(Passkey<GameEngine>)
    : keyboard_(std::make_unique<Keyboard>(Passkey<Input>{}))
    , mouse_(std::make_unique<Mouse>(Passkey<Input>{}))
    , controller_(std::make_unique<Controller>(Passkey<Input>{})) {
    if (keyboard_) {
        keyboard_->Initialize();
    }
    if (mouse_) {
        mouse_->Initialize();
    }
    if (controller_) {
        controller_->Initialize();
    }
    ResetDeviceThresholds();
}

Input::~Input() {
    if (controller_) {
        controller_->Finalize();
    }
    if (mouse_) {
        mouse_->Finalize();
    }
    if (keyboard_) {
        keyboard_->Finalize();
    }
}

void Input::Update() {
    if (keyboard_) {
        keyboard_->Update();
    }
    if (mouse_) {
        mouse_->Update();
    }
    if (controller_) {
        controller_->Update();
    }

    UpdateActiveDevice_();
}

namespace {
/// @brief この間隔以上マウスが静止したら、累積移動量を別の動作として数え直す
constexpr std::chrono::milliseconds kMouseMoveResetInterval{ 150 };

/// @brief 前面のウィンドウがこのプロセス（ゲーム/エディターのいずれか）のものかを判定する
/// @details GameInput / GetAsyncKeyState は非アクティブ時も入力を拾うため、
///          他アプリの操作で入力デバイスが切り替わらないようにする
bool IsApplicationForeground() {
    const HWND foreground = GetForegroundWindow();
    if (!foreground) return false;
    DWORD processId = 0;
    GetWindowThreadProcessId(foreground, &processId);
    return processId == GetCurrentProcessId();
}
} // namespace

void Input::UpdateActiveDevice_() {
    activeDeviceChanged_ = false;
    if (!keyboard_ || !mouse_ || !controller_) return;

    bool keyboardMouseActive = false;
    int activePad = -1;
    if (IsApplicationForeground()) {
        keyboardMouseActive = DetectKeyboardMouseActivity_();
        activePad = DetectControllerActivity_();
    } else {
        mouseMoveAccumulated_ = 0.0f;
    }

    // 切断等でインデックスが範囲外になったコントローラーは無効扱いにする
    if (activeControllerIndex_ >= controller_->GetPadCount()) {
        activeControllerIndex_ = -1;
    }
    if (activePad >= 0) {
        activeControllerIndex_ = activePad;
    }

    InputDeviceType next = activeDevice_;
    if (keyboardMouseActive && activePad >= 0) {
        // 同一フレームで両方に入力がある場合は、現在のデバイスを維持して頻繁な切り替わりを防ぐ
    } else if (keyboardMouseActive) {
        next = InputDeviceType::KeyboardMouse;
    } else if (activePad >= 0) {
        next = InputDeviceType::Controller;
    } else if (activeDevice_ == InputDeviceType::Controller && controller_->GetPadCount() == 0) {
        // コントローラーがすべて切断された場合はキーボード＋マウスへ戻す
        next = InputDeviceType::KeyboardMouse;
    }

    if (next != activeDevice_) {
        activeDevice_ = next;
        activeDeviceChanged_ = true;
    }
}

bool Input::DetectKeyboardMouseActivity_() {
    bool active = keyboard_->IsAnyTrigger();

    for (int i = 0; i < 8; ++i) {
        if (mouse_->IsButtonTrigger(i)) {
            active = true;
            break;
        }
    }

    if (mouse_->GetWheel() != 0) {
        active = true;
    }

    // マウスの小さな揺れで切り替わらないよう、一連の移動の累積量がしきい値を超えた場合のみ有効とする
    const int dx = mouse_->GetDeltaX();
    const int dy = mouse_->GetDeltaY();
    if (dx != 0 || dy != 0) {
        const auto now = std::chrono::steady_clock::now();
        if (now - lastMouseMoveTime_ > kMouseMoveResetInterval) {
            mouseMoveAccumulated_ = 0.0f;
        }
        lastMouseMoveTime_ = now;
        mouseMoveAccumulated_ += std::sqrt(static_cast<float>(dx) * static_cast<float>(dx) + static_cast<float>(dy) * static_cast<float>(dy));
        if (mouseMoveAccumulated_ >= mouseMoveThreshold_) {
            active = true;
        }
    }

    return active;
}

int Input::DetectControllerActivity_() const {
    const auto pads = controller_->GetPads();
    const auto prevPads = controller_->GetPrevPads();

    const float stickThreshold = controllerStickThreshold_ * 32767.0f;
    const float triggerThreshold = controllerTriggerThreshold_ * 255.0f;

    const auto isPadActive = [&](int index) -> bool {
        if (index < 0 || index >= static_cast<int>(pads.size())) return false;
        if (!controller_->IsConnected(index)) return false;

        const auto &pad = pads[static_cast<size_t>(index)];
        const std::uint16_t prevButtons = (static_cast<size_t>(index) < prevPads.size()) ? prevPads[static_cast<size_t>(index)].buttons : 0;
        if ((pad.buttons & ~prevButtons) != 0) return true;

        // しきい値が0の場合でも無入力（0）は入力扱いにしない
        const auto exceeds = [](float value, float threshold) {
            return value > 0.0f && value >= threshold;
        };
        if (exceeds(static_cast<float>(pad.leftTrigger), triggerThreshold)) return true;
        if (exceeds(static_cast<float>(pad.rightTrigger), triggerThreshold)) return true;

        const auto stickLength = [](std::int16_t x, std::int16_t y) {
            return std::sqrt(static_cast<float>(x) * static_cast<float>(x) + static_cast<float>(y) * static_cast<float>(y));
        };
        if (exceeds(stickLength(pad.leftX, pad.leftY), stickThreshold)) return true;
        if (exceeds(stickLength(pad.rightX, pad.rightY), stickThreshold)) return true;

        return false;
    };

    if (isPadActive(activeControllerIndex_)) {
        return activeControllerIndex_;
    }
    for (int i = 0; i < static_cast<int>(pads.size()); ++i) {
        if (isPadActive(i)) return i;
    }
    return -1;
}

void Input::SetMouseMoveThreshold(float pixels) noexcept {
    mouseMoveThreshold_ = (std::max)(pixels, 0.0f);
}

void Input::SetControllerStickThreshold(float threshold) noexcept {
    controllerStickThreshold_ = std::clamp(threshold, 0.0f, 1.0f);
}

void Input::SetControllerTriggerThreshold(float threshold) noexcept {
    controllerTriggerThreshold_ = std::clamp(threshold, 0.0f, 1.0f);
}

void Input::ResetDeviceThresholds() {
    const auto &settings = GetEngineSettings().input;
    SetMouseMoveThreshold(settings.mouseMoveThreshold);
    SetControllerStickThreshold(settings.controllerStickThreshold);
    SetControllerTriggerThreshold(settings.controllerTriggerThreshold);
}

Keyboard& Input::GetKeyboard() {
    return *keyboard_;
}

const Keyboard& Input::GetKeyboard() const {
    return *keyboard_;
}

Mouse& Input::GetMouse() {
    return *mouse_;
}

const Mouse& Input::GetMouse() const {
    return *mouse_;
}

Controller& Input::GetController() {
    return *controller_;
}

const Controller& Input::GetController() const {
    return *controller_;
}

#if defined(USE_IMGUI)
namespace {
struct KeyEntry {
    Key key;
    const char* label;
};

constexpr KeyEntry kKeyEntries[] = {
    { Key::W, "W" },
    { Key::A, "A" },
    { Key::S, "S" },
    { Key::D, "D" },
    { Key::Space, "Space" },
    { Key::Shift, "Shift" },
    { Key::Control, "Ctrl" },
    { Key::Alt, "Alt" },
    { Key::Left, "Left" },
    { Key::Right, "Right" },
    { Key::Up, "Up" },
    { Key::Down, "Down" },
    { Key::Enter, "Enter" },
    { Key::Escape, "Esc" },
    { Key::Tab, "Tab" },
    { Key::Backspace, "Backspace" },
    { Key::F1, "F1" },
    { Key::F2, "F2" },
    { Key::F3, "F3" },
    { Key::F4, "F4" },
    { Key::F5, "F5" },
    { Key::F6, "F6" },
    { Key::F7, "F7" },
    { Key::F8, "F8" },
    { Key::F9, "F9" },
    { Key::F10, "F10" },
    { Key::F11, "F11" },
    { Key::F12, "F12" },
};

float NormalizeStick(int v) {
    // Controller は -32767..32767 相当
    constexpr float denom = 32767.0f;
    float f = static_cast<float>(v) / denom;
    if (f > 1.0f) f = 1.0f;
    if (f < -1.0f) f = -1.0f;
    return f;
}

float NormalizeTrigger(int v) {
    // Controller は 0..255
    constexpr float denom = 255.0f;
    float f = static_cast<float>(v) / denom;
    if (f > 1.0f) f = 1.0f;
    if (f < 0.0f) f = 0.0f;
    return f;
}
} // namespace

void Input::ShowImGui() {
    if (!keyboard_ || !mouse_ || !controller_) return;

    if (!ImGui::Begin(TranslationLabel("editor.input.state.window"))) {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader(TranslationLabel("editor.input.active_device"), ImGuiTreeNodeFlags_DefaultOpen)) {
        const char *deviceName = IsUsingController()
            ? TranslationC("editor.input.device_controller")
            : TranslationC("editor.input.device_keyboard_mouse");
        ImGui::Text(TranslationC("editor.input.active_device_s"), deviceName);
        ImGui::Text(TranslationC("editor.input.active_controller_index_d"), activeControllerIndex_);

        float mouseMoveThreshold = mouseMoveThreshold_;
        if (ImGui::DragFloat(TranslationLabel("editor.input.mouse_move_threshold"), &mouseMoveThreshold, 0.5f, 0.0f, 500.0f, "%.1f")) {
            SetMouseMoveThreshold(mouseMoveThreshold);
        }
        float stickThreshold = controllerStickThreshold_;
        if (ImGui::SliderFloat(TranslationLabel("editor.input.stick_threshold"), &stickThreshold, 0.0f, 1.0f, "%.2f")) {
            SetControllerStickThreshold(stickThreshold);
        }
        float triggerThreshold = controllerTriggerThreshold_;
        if (ImGui::SliderFloat(TranslationLabel("editor.input.trigger_threshold"), &triggerThreshold, 0.0f, 1.0f, "%.2f")) {
            SetControllerTriggerThreshold(triggerThreshold);
        }
    }

    if (ImGui::CollapsingHeader(TranslationLabel("editor.input.keyboard"), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextUnformatted(TranslationC("editor.input.desc_1"));
        for (const auto& e : kKeyEntries) {
            const bool down = keyboard_->IsDown(e.key);
            const bool trig = keyboard_->IsTrigger(e.key);
            const bool rel = keyboard_->IsRelease(e.key);
            ImGui::Text(TranslationC("editor.input.10s_down_s_trg_s_rel_s"), e.label, down ? "1" : "0", trig ? "1" : "0", rel ? "1" : "0");
        }
    }

    if (ImGui::CollapsingHeader(TranslationLabel("editor.input.mouse"), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text(TranslationC("editor.input.delta_d_d"), mouse_->GetDeltaX(), mouse_->GetDeltaY());
        ImGui::Text(TranslationC("editor.input.wheel_delta_d"), mouse_->GetWheel());

        ImGui::Separator();
        ImGui::TextUnformatted(TranslationC("editor.input.buttons"));
        for (int i = 0; i < 8; ++i) {
            ImGui::PushID(i);
            const bool down = mouse_->IsButtonDown(i);
            const bool trig = mouse_->IsButtonTrigger(i);
            const bool rel = mouse_->IsButtonRelease(i);
            ImGui::Text(TranslationC("editor.input.button_d_down_s_trg_s_rel_s"), i, down ? "1" : "0", trig ? "1" : "0", rel ? "1" : "0");
            ImGui::PopID();
        }

        ImGui::Separator();
        ImGui::TextUnformatted(TranslationC("editor.input.cursor"));
        const POINT p = mouse_->GetPos(static_cast<HWND>(nullptr));
        ImGui::Text(TranslationC("editor.input.screen_pos_ld_ld"), p.x, p.y);
    }

    if (ImGui::CollapsingHeader(TranslationLabel("editor.input.controller_gamepad"), ImGuiTreeNodeFlags_DefaultOpen)) {
        const int padCount = controller_->GetPadCount();
        ImGui::Text(TranslationC("editor.input.pads_d"), padCount);

        const auto showButtonRow = [&](int idx, const char* name, ControllerButton btn) {
            const bool down = controller_->IsButtonDown(btn, idx);
            const bool trig = controller_->IsButtonTrigger(btn, idx);
            const bool rel = controller_->IsButtonRelease(btn, idx);
            ImGui::Text(TranslationC("editor.input.10s_down_s_trg_s_rel_s_2"), name, down ? "1" : "0", trig ? "1" : "0", rel ? "1" : "0");
        };

        for (int i = 0; i < padCount; ++i) {
            ImGui::PushID(i);
            const bool connected = controller_->IsConnected(i);
            ImGui::Text(TranslationC("editor.input.pad_d_s"), i, connected ? "Connected" : "Disconnected");
            if (connected) {
                const int lt = controller_->GetLeftTrigger(i);
                const int rt = controller_->GetRightTrigger(i);

                const float nlt = NormalizeTrigger(lt);
                const float nrt = NormalizeTrigger(rt);

                ImGui::Text(TranslationC("editor.input.trigger_l_d_2f_r_d_2f"), lt, nlt, rt, nrt);

                const float lx = NormalizeStick(controller_->GetLeftStickX(i));
                const float ly = NormalizeStick(controller_->GetLeftStickY(i));
                const float rx = NormalizeStick(controller_->GetRightStickX(i));
                const float ry = NormalizeStick(controller_->GetRightStickY(i));

                ImGui::Text(TranslationC("editor.input.leftstick_2f_2f"), lx, ly);
                ImGui::Text(TranslationC("editor.input.rightstick_2f_2f"), rx, ry);

                ImGui::SeparatorText(TranslationLabel("editor.input.buttons"));
                showButtonRow(i, "A", ControllerButton::A);
                showButtonRow(i, "B", ControllerButton::B);
                showButtonRow(i, "X", ControllerButton::X);
                showButtonRow(i, "Y", ControllerButton::Y);
                showButtonRow(i, "LB", ControllerButton::LeftShoulder);
                showButtonRow(i, "RB", ControllerButton::RightShoulder);
                showButtonRow(i, "Back", ControllerButton::Back);
                showButtonRow(i, "Start", ControllerButton::Start);
                showButtonRow(i, "LThumb", ControllerButton::LeftThumb);
                showButtonRow(i, "RThumb", ControllerButton::RightThumb);
                showButtonRow(i, "Up", ControllerButton::DPadUp);
                showButtonRow(i, "Down", ControllerButton::DPadDown);
                showButtonRow(i, "Left", ControllerButton::DPadLeft);
                showButtonRow(i, "Right", ControllerButton::DPadRight);
            }
            ImGui::Separator();
            ImGui::PopID();
        }
    }

    ImGui::End();
}
#endif

} // namespace KashipanEngine
