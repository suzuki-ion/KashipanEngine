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

    auto *mainWIndow = Window::CreateNormal("Main Window", 1920, 1080);
#if defined(DEBUG_BUILD) or defined(DEVELOPMENT_BUILD)
    mainWIndow->UnregisterWindowEvent(WM_SYSCOMMAND);
    mainWIndow->RegisterWindowEvent<WindowDefaultEvent::SysCommandCloseEventSimple>();
#endif

    if (context.sceneManager) {
        auto *sm = context.sceneManager;
        
        //sm->RegisterScene<EngineLogoScene>("EngineLogoScene", "");
        //sm->RegisterScene<TitleScene>("TitleScene");
        sm->RegisterScene<GameScene>("GameScene");
        //sm->RegisterScene<ResultScene>("ResultScene");
        //sm->RegisterScene<GameOverScene>("GameOverScene");

        #if defined(DEBUG_BUILD) || defined(DEVELOPMENT_BUILD)
                sm->RegisterScene<TestScene>("TestScene");
                context.sceneManager->ChangeScene("TestScene");
        #endif
		context.sceneManager->ChangeScene("TestScene");
    }

    if (context.inputCommand) {
        auto *ic = context.inputCommand;
        ic->Clear();

        // 左右移動: A/D or 左右矢印, コントローラー左スティックX/十字キー
        ic->RegisterCommand("MoveLeft", InputCommand::KeyboardKey{ Key::A }, InputCommand::InputState::Down);
        ic->RegisterCommand("MoveLeft", InputCommand::KeyboardKey{ Key::Left }, InputCommand::InputState::Down);
        ic->RegisterCommand("MoveLeft", ControllerButton::DPadLeft, InputCommand::InputState::Down);
        ic->RegisterCommand("MoveLeft", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Down, 0, -0.1f);
        ic->RegisterCommand("MoveRight", InputCommand::KeyboardKey{ Key::D }, InputCommand::InputState::Down);
        ic->RegisterCommand("MoveRight", InputCommand::KeyboardKey{ Key::Right }, InputCommand::InputState::Down);
        ic->RegisterCommand("MoveRight", ControllerButton::DPadRight, InputCommand::InputState::Down);
        ic->RegisterCommand("MoveRight", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Down, 0, 0.1f);
        
        // 蹴る: W/上矢印(前蹴り), S/下矢印(下蹴り)
        ic->RegisterCommand("KickFront", InputCommand::KeyboardKey{ Key::W }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("KickFront", InputCommand::KeyboardKey{ Key::Up }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("KickFront", ControllerButton::DPadUp, InputCommand::InputState::Trigger);
        ic->RegisterCommand("KickFront", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Trigger, 0, 0.1f);
        ic->RegisterCommand("KickDown", InputCommand::KeyboardKey{ Key::S }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("KickDown", InputCommand::KeyboardKey{ Key::Down }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("KickDown", ControllerButton::DPadDown, InputCommand::InputState::Trigger);
        ic->RegisterCommand("KickDown", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Trigger, 0, -0.1f);

        // 旋回: Q/E or J/K, コントローラー右スティックX/LT/RT
        ic->RegisterCommand("TurnLeft", InputCommand::KeyboardKey{ Key::Q }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("TurnLeft", InputCommand::KeyboardKey{ Key::J }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("TurnLeft", InputCommand::ControllerAnalog::RightStickX, InputCommand::InputState::Trigger, 0, -0.5f);
        ic->RegisterCommand("TurnLeft", InputCommand::ControllerAnalog::LeftTrigger, InputCommand::InputState::Trigger, 0, 0.5f);
        ic->RegisterCommand("TurnRight", InputCommand::KeyboardKey{ Key::E }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("TurnRight", InputCommand::KeyboardKey{ Key::K }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("TurnRight", InputCommand::ControllerAnalog::RightStickX, InputCommand::InputState::Trigger, 0, 0.5f);
        ic->RegisterCommand("TurnRight", InputCommand::ControllerAnalog::RightTrigger, InputCommand::InputState::Trigger, 0, 0.5f);

		// 決定
        ic->RegisterCommand("Submit", InputCommand::KeyboardKey{ Key::Enter }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Submit", InputCommand::KeyboardKey{ Key::Space }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Submit", ControllerButton::A, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Submit", ControllerButton::Start, InputCommand::InputState::Trigger);
        // キャンセル
        ic->RegisterCommand("Cancel", InputCommand::KeyboardKey{ Key::Escape }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Cancel", ControllerButton::B, InputCommand::InputState::Trigger);
		ic->RegisterCommand("Cancel", ControllerButton::Back, InputCommand::InputState::Trigger);

        // デバッグ用シーン遷移
        ic->RegisterCommand("DebugSceneChange", InputCommand::KeyboardKey{ Key::F1 }, InputCommand::InputState::Trigger);

        // パズルゲーム用入力
        ic->RegisterCommand("PuzzleUp", InputCommand::KeyboardKey{ Key::W }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleUp", InputCommand::KeyboardKey{ Key::Up }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleUp", ControllerButton::DPadUp, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleUp", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Trigger, 0, 0.5f);

        ic->RegisterCommand("PuzzleDown", InputCommand::KeyboardKey{ Key::S }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleDown", InputCommand::KeyboardKey{ Key::Down }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleDown", ControllerButton::DPadDown, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleDown", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Trigger, 0, -0.5f);

        ic->RegisterCommand("PuzzleLeft", InputCommand::KeyboardKey{ Key::A }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleLeft", InputCommand::KeyboardKey{ Key::Left }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleLeft", ControllerButton::DPadLeft, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleLeft", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Trigger, 0, -0.5f);

        ic->RegisterCommand("PuzzleRight", InputCommand::KeyboardKey{ Key::D }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleRight", InputCommand::KeyboardKey{ Key::Right }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleRight", ControllerButton::DPadRight, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleRight", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Trigger, 0, 0.5f);

        ic->RegisterCommand("PuzzleAction", InputCommand::KeyboardKey{ Key::Space }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleAction", ControllerButton::A, InputCommand::InputState::Trigger);
    }
}

} // namespace KashipanEngine
