#pragma once
#include <KashipanEngine.h>

#include "Scenes/TitleScene.h"
#include "Scenes/TestScene.h"

namespace KashipanEngine {

inline void AppInitialize(const GameEngine::Context &context) {
    auto monitorInfoOpt = WindowsAPI::QueryMonitorInfo();
    const RECT area = monitorInfoOpt ? monitorInfoOpt->WorkArea() : RECT{ 0, 0, 1280, 720 };

    Window::CreateNormal("Main Window", 1920, 1080);
    /*auto *overlay = Window::CreateOverlay("Overlay Window",
        area.right, area.bottom, true);
    if (overlay) {
        overlay->RegisterWindowEvent<WindowDefaultEvent::SysCommandCloseEventSimple>();
    }*/

    if (context.sceneManager) {
        //context.sceneManager->RegisterScene<TitleScene>("TitleScene");
        context.sceneManager->RegisterScene<TestScene>("TestScene");
        /*context.sceneManager->RegisterScene<GameScene>("GameScene");
        context.sceneManager->RegisterScene<GameOverScene>("GameOverScene");
        context.sceneManager->RegisterScene<ResultScene>("ResultScene");*/

        context.sceneManager->ChangeScene("TestScene");
    }

    if (context.inputCommand) {
        auto *ic = context.inputCommand;
        ic->Clear();

        // 移動
        ic->RegisterCommand("MoveX", InputCommand::KeyboardKey{ Key::A }, InputCommand::InputState::Down, true);
        ic->RegisterCommand("MoveX", InputCommand::KeyboardKey{ Key::D }, InputCommand::InputState::Down);
        ic->RegisterCommand("MoveX", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Down);

        ic->RegisterCommand("MoveZ", InputCommand::KeyboardKey{ Key::W }, InputCommand::InputState::Down);
        ic->RegisterCommand("MoveZ", InputCommand::KeyboardKey{ Key::S }, InputCommand::InputState::Down, true);
        ic->RegisterCommand("MoveZ", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Down);

        // 攻撃
        ic->RegisterCommand("AttackCharge", InputCommand::KeyboardKey{ Key::Space }, InputCommand::InputState::Down);
        ic->RegisterCommand("AttackCharge", InputCommand::ControllerAnalog::RightTrigger, InputCommand::InputState::Down);

        ic->RegisterCommand("Attack", InputCommand::KeyboardKey{ Key::Space }, InputCommand::InputState::Release);
        ic->RegisterCommand("Attack", InputCommand::ControllerAnalog::RightTrigger, InputCommand::InputState::Release);

        // ダッシュ
        ic->RegisterCommand("Dash", InputCommand::KeyboardKey{ Key::Shift }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Dash", ControllerButton::X, InputCommand::InputState::Trigger);

        // 決定
        ic->RegisterCommand("Submit", InputCommand::KeyboardKey{ Key::Enter }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Submit", InputCommand::KeyboardKey{ Key::Space }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Submit", ControllerButton::A, InputCommand::InputState::Trigger);
    }
}

} // namespace KashipanEngine
