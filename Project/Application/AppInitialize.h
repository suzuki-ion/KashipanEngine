#pragma once
#include <KashipanEngine.h>
#include "Scenes/EngineLogoScene.h"
#include "Scenes/TitleScene.h"
#if defined(DEBUG_BUILD) or defined(DEVELOPMENT_BUILD)
#include "Scenes/TestScene.h"
#endif
#include "Scenes/GameScene.h"
#include "Scenes/ResultScene.h"
#include "Scenes/GameOverScene.h"

namespace KashipanEngine {

inline void AppInitialize(const GameEngine::Context &context) {
    auto monitorInfoOpt = WindowsAPI::QueryMonitorInfo();
    const RECT area = monitorInfoOpt ? monitorInfoOpt->WorkArea() : RECT{ 0, 0, 1280, 720 };

    Window::CreateNormal("Main Window", 1920, 1080);

    if (context.sceneManager) {
        auto *sm = context.sceneManager;
        
        //sm->RegisterScene<EngineLogoScene>("EngineLogoScene", "");
        //sm->RegisterScene<TitleScene>("TitleScene");
        sm->RegisterScene<GameScene>("GameScene");
        //sm->RegisterScene<ResultScene>("ResultScene");
        //sm->RegisterScene<GameOverScene>("GameOverScene");

        //#if defined(DEBUG_BUILD) || defined(DEVELOPMENT_BUILD)
        //        sm->RegisterScene<TestScene>("TestScene");
        //        context.sceneManager->ChangeScene("TestScene");
        //#endif
		context.sceneManager->ChangeScene("GameScene");
    }

    if (context.inputCommand) {
        auto *ic = context.inputCommand;
        ic->Clear();

        // 左右移動
        ic->RegisterCommand("MoveLeft", InputCommand::KeyboardKey{ Key::A }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveLeft", InputCommand::KeyboardKey{ Key::Left }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveLeft", ControllerButton::DPadLeft, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveLeft", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Trigger, 0, -0.1f);
        ic->RegisterCommand("MoveRight", InputCommand::KeyboardKey{ Key::D }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveRight", InputCommand::KeyboardKey{ Key::Right }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveRight", ControllerButton::DPadRight, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveRight", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Trigger, 0, 0.1f);
        
        // 上下移動
        ic->RegisterCommand("MoveUp", InputCommand::KeyboardKey{ Key::W }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveUp", InputCommand::KeyboardKey{ Key::Up }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveUp", ControllerButton::DPadUp, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveUp", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Trigger, 0, -0.1f);
        ic->RegisterCommand("MoveDown", InputCommand::KeyboardKey{ Key::S }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveDown", InputCommand::KeyboardKey{ Key::Down }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveDown", ControllerButton::DPadDown, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveDown", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Trigger, 0, 0.1f);

		// 決定
        ic->RegisterCommand("Submit", InputCommand::KeyboardKey{ Key::Enter }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Submit", InputCommand::KeyboardKey{ Key::Space }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Submit", ControllerButton::A, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Submit", ControllerButton::Start, InputCommand::InputState::Trigger);
        // キャンセル
        ic->RegisterCommand("Cancel", InputCommand::KeyboardKey{ Key::Escape }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Cancel", ControllerButton::B, InputCommand::InputState::Trigger);
		ic->RegisterCommand("Cancel", ControllerButton::Back, InputCommand::InputState::Trigger);
    }
}

} // namespace KashipanEngine
