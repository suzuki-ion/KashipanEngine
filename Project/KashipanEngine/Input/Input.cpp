#include "Input/Input.h"

#include "Input/Keyboard.h"
#include "Input/Mouse.h"
#include "Input/Controller.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#include <cctype>
#include <string>
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

    if (!ImGui::Begin("Input - 入力状態")) {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader("Keyboard", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextUnformatted("主要キー状態");
        for (const auto& e : kKeyEntries) {
            const bool down = keyboard_->IsDown(e.key);
            const bool trig = keyboard_->IsTrigger(e.key);
            const bool rel = keyboard_->IsRelease(e.key);
            ImGui::Text("%-10s  Down:%s  Trg:%s  Rel:%s", e.label, down ? "1" : "0", trig ? "1" : "0", rel ? "1" : "0");
        }
    }

    if (ImGui::CollapsingHeader("Mouse", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Delta: (%d, %d)", mouse_->GetDeltaX(), mouse_->GetDeltaY());
        ImGui::Text("Wheel(Delta): %d", mouse_->GetWheel());

        ImGui::Separator();
        ImGui::TextUnformatted("Buttons");
        for (int i = 0; i < 8; ++i) {
            ImGui::PushID(i);
            const bool down = mouse_->IsButtonDown(i);
            const bool trig = mouse_->IsButtonTrigger(i);
            const bool rel = mouse_->IsButtonRelease(i);
            ImGui::Text("Button %d  Down:%s  Trg:%s  Rel:%s", i, down ? "1" : "0", trig ? "1" : "0", rel ? "1" : "0");
            ImGui::PopID();
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Cursor");
        const POINT p = mouse_->GetPos(static_cast<HWND>(nullptr));
        ImGui::Text("Screen Pos: (%ld, %ld)", p.x, p.y);
    }

    if (ImGui::CollapsingHeader("Controller (Gamepad)", ImGuiTreeNodeFlags_DefaultOpen)) {
        const int padCount = controller_->GetPadCount();
        ImGui::Text("Pads: %d", padCount);

        const auto showButtonRow = [&](int idx, const char* name, ControllerButton btn) {
            const bool down = controller_->IsButtonDown(btn, idx);
            const bool trig = controller_->IsButtonTrigger(btn, idx);
            const bool rel = controller_->IsButtonRelease(btn, idx);
            ImGui::Text("%-10s Down:%s Trg:%s Rel:%s", name, down ? "1" : "0", trig ? "1" : "0", rel ? "1" : "0");
        };

        for (int i = 0; i < padCount; ++i) {
            ImGui::PushID(i);
            const bool connected = controller_->IsConnected(i);
            ImGui::Text("Pad %d: %s", i, connected ? "Connected" : "Disconnected");
            if (connected) {
                const int lt = controller_->GetLeftTrigger(i);
                const int rt = controller_->GetRightTrigger(i);

                const float nlt = NormalizeTrigger(lt);
                const float nrt = NormalizeTrigger(rt);

                ImGui::Text("Trigger L:%d (%.2f)  R:%d (%.2f)", lt, nlt, rt, nrt);

                const float lx = NormalizeStick(controller_->GetLeftStickX(i));
                const float ly = NormalizeStick(controller_->GetLeftStickY(i));
                const float rx = NormalizeStick(controller_->GetRightStickX(i));
                const float ry = NormalizeStick(controller_->GetRightStickY(i));

                ImGui::Text("LeftStick : (%.2f, %.2f)", lx, ly);
                ImGui::Text("RightStick: (%.2f, %.2f)", rx, ry);

                ImGui::SeparatorText("Buttons");
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
