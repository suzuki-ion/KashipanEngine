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

        /*#if defined(DEBUG_BUILD) || defined(DEVELOPMENT_BUILD)
                sm->RegisterScene<TestScene>("TestScene");
                context.sceneManager->ChangeScene("TestScene");
        #endif*/
		context.sceneManager->ChangeScene("GameScene");
    }

    if (context.inputCommand) {
        auto *ic = context.inputCommand;
        ic->Clear();

        // * ゲーム外のコマンド * //
        // 決定
        ic->RegisterCommand("Submit", Key::Enter, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Submit", Key::Space, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Submit", ControllerButton::A, InputCommand::InputState::Trigger);
        
        // キャンセル
        ic->RegisterCommand("Cancel", Key::Escape, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Cancel", ControllerButton::B, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Cancel", ControllerButton::Back, InputCommand::InputState::Trigger);

		// メニュー呼び出し
		ic->RegisterCommand("Menu", Key::M, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Menu", Key::E, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Menu", ControllerButton::Start, InputCommand::InputState::Trigger);

		// 上下選択
		ic->RegisterCommand("Up", Key::Up, InputCommand::InputState::Trigger);
		ic->RegisterCommand("Up", ControllerButton::DPadUp, InputCommand::InputState::Trigger);
		ic->RegisterCommand("Down", Key::Down, InputCommand::InputState::Trigger);
		ic->RegisterCommand("Down", ControllerButton::DPadDown, InputCommand::InputState::Trigger);

        // デバッグ用シーン遷移
        ic->RegisterCommand("DebugSceneChange", Key::F1, InputCommand::InputState::Trigger);

		// * ゲームプレイ用入力コマンド * //
        // 左右移動: A/D or 左右矢印, コントローラー左スティックX/十字キー
        ic->RegisterCommand("MoveLeft", Key::A, InputCommand::InputState::Down);
        ic->RegisterCommand("MoveLeft", Key::Left, InputCommand::InputState::Down);
        ic->RegisterCommand("MoveLeft", ControllerButton::DPadLeft, InputCommand::InputState::Down);
        ic->RegisterCommand("MoveLeft", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Down, 0, -0.1f);
        ic->RegisterCommand("MoveRight", Key::D, InputCommand::InputState::Down);
        ic->RegisterCommand("MoveRight", Key::Right, InputCommand::InputState::Down);
        ic->RegisterCommand("MoveRight", ControllerButton::DPadRight, InputCommand::InputState::Down);
        ic->RegisterCommand("MoveRight", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Down, 0, 0.1f);
        
        // 蹴る: W/上矢印(前蹴り), S/下矢印(下蹴り)
        ic->RegisterCommand("KickFront", Key::W, InputCommand::InputState::Trigger);
        ic->RegisterCommand("KickFront", Key::Up, InputCommand::InputState::Trigger);
        ic->RegisterCommand("KickFront", ControllerButton::DPadUp, InputCommand::InputState::Trigger);
        ic->RegisterCommand("KickFront", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Trigger, 0, 0.1f);
        ic->RegisterCommand("KickDown", Key::S, InputCommand::InputState::Trigger);
        ic->RegisterCommand("KickDown", Key::Down, InputCommand::InputState::Trigger);
        ic->RegisterCommand("KickDown", ControllerButton::DPadDown, InputCommand::InputState::Trigger);
        ic->RegisterCommand("KickDown", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Trigger, 0, -0.1f);

        // 旋回: Q/E or J/K, コントローラー右スティックX/LT/RT
        ic->RegisterCommand("TurnLeft", Key::Q, InputCommand::InputState::Trigger);
        ic->RegisterCommand("TurnLeft", Key::J, InputCommand::InputState::Trigger);
        ic->RegisterCommand("TurnLeft", InputCommand::ControllerAnalog::RightStickX, InputCommand::InputState::Trigger, 0, -0.5f);
        ic->RegisterCommand("TurnLeft", InputCommand::ControllerAnalog::LeftTrigger, InputCommand::InputState::Trigger, 0, 0.5f);
        ic->RegisterCommand("TurnRight", Key::E, InputCommand::InputState::Trigger);
        ic->RegisterCommand("TurnRight", Key::K, InputCommand::InputState::Trigger);
        ic->RegisterCommand("TurnRight", InputCommand::ControllerAnalog::RightStickX, InputCommand::InputState::Trigger, 0, 0.5f);
        ic->RegisterCommand("TurnRight", InputCommand::ControllerAnalog::RightTrigger, InputCommand::InputState::Trigger, 0, 0.5f);

        // パズルゲーム用入力
        ic->RegisterCommand("PuzzleUp", Key::W, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleUp", Key::Up, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleUp", ControllerButton::DPadUp, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleUp", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Trigger, 0, 0.5f);

        ic->RegisterCommand("PuzzleDown", Key::S, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleDown", Key::Down, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleDown", ControllerButton::DPadDown, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleDown", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Trigger, 0, -0.5f);

        ic->RegisterCommand("PuzzleLeft", Key::A, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleLeft", Key::Left, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleLeft", ControllerButton::DPadLeft, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleLeft", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Trigger, 0, -0.5f);

        ic->RegisterCommand("PuzzleRight", Key::D, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleRight", Key::Right, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleRight", ControllerButton::DPadRight, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleRight", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Trigger, 0, 0.5f);

        ic->RegisterCommand("PuzzleAction", Key::Space, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleAction", ControllerButton::A, InputCommand::InputState::Trigger);

        // パズルパネル移動アクション（押しっぱなし判定用）
        ic->RegisterCommand("PuzzleActionHold", Key::Space, InputCommand::InputState::Down);
        ic->RegisterCommand("PuzzleActionHold", ControllerButton::A, InputCommand::InputState::Down);
    }
}

} // namespace KashipanEngine
