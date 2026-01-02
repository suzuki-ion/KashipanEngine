#include "Input/Input.h"

#include "Input/Keyboard.h"
#include "Input/Mouse.h"
#include "Input/Controller.h"

namespace KashipanEngine {

Input::Input()
    : keyboard_(std::make_unique<Keyboard>())
    , mouse_(std::make_unique<Mouse>())
    , controller_(std::make_unique<Controller>()) {
}

Input::~Input() = default;

void Input::Initialize(HINSTANCE hInstance, HWND hwnd) {
    if (keyboard_) {
        keyboard_->Initialize(hInstance, hwnd);
    }
    if (mouse_) {
        mouse_->Initialize(hInstance);
    }
    if (controller_) {
        controller_->Initialize();
    }
}

void Input::Finalize() {
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

} // namespace KashipanEngine
