#pragma once
#include <KashipanEngine.h>

#include "Scenes/TitleScene.h"
#include "Scenes/TestScene.h"
#include "Scenes/GameScene.h"
#include "Scenes/ResultScene.h"
#include "Scenes/GameOverScene.h"

namespace KashipanEngine {

inline void AppInitialize(const GameEngine::Context &context) {
    auto monitorInfoOpt = WindowsAPI::QueryMonitorInfo();
    const RECT area = monitorInfoOpt ? monitorInfoOpt->WorkArea() : RECT{ 0, 0, 1280, 720 };

    Window::CreateNormal("2301_CLUBOM", 1920, 1280);

    if (context.sceneManager) {
        auto *sm = context.sceneManager;
        
        sm->RegisterScene<TitleScene>("TitleScene");
        sm->RegisterScene<GameScene>("GameScene");
        sm->RegisterScene<ResultScene>("ResultScene");
        sm->RegisterScene<GameOverScene>("GameOverScene");

        sm->RegisterScene<TestScene>("TestScene");

        context.sceneManager->ChangeScene("GameScene");
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

        ic->RegisterCommand("MoveUp", InputCommand::KeyboardKey{ Key::W }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveUp", InputCommand::KeyboardKey{ Key::Up }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveUp", ControllerButton::DPadUp, InputCommand::InputState::Trigger);
        
        ic->RegisterCommand("MoveDown", InputCommand::KeyboardKey{ Key::S }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveDown", InputCommand::KeyboardKey{ Key::Down }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveDown", ControllerButton::DPadDown, InputCommand::InputState::Trigger);
        
        ic->RegisterCommand("MoveLeft", InputCommand::KeyboardKey{ Key::A }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveLeft", InputCommand::KeyboardKey{ Key::Left }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveLeft", ControllerButton::DPadLeft, InputCommand::InputState::Trigger);
        
        ic->RegisterCommand("MoveRight", InputCommand::KeyboardKey{ Key::D }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveRight", InputCommand::KeyboardKey{ Key::Right }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("MoveRight", ControllerButton::DPadRight, InputCommand::InputState::Trigger);

        ic->RegisterCommand("Bomb", InputCommand::KeyboardKey{ Key::Space }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Bomb", InputCommand::KeyboardKey{ Key::Z }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Bomb", ControllerButton::A, InputCommand::InputState::Trigger);

        ic->RegisterCommand("ModeChange", InputCommand::KeyboardKey{ Key::D1 }, InputCommand::InputState::Trigger);
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

#ifdef DEBUG_BUILD
        // デバッグ用シーン遷移
        ic->RegisterCommand("DebugSceneChange", InputCommand::KeyboardKey{ Key::F1 }, InputCommand::InputState::Trigger);
        // デバッグ用バックモニター描画切り替え
        ic->RegisterCommand("DebugChangeBackMonitor", InputCommand::KeyboardKey{ Key::F2 }, InputCommand::InputState::Trigger);
        // デバッグ用リセットコマンド
        ic->RegisterCommand("DebugReset", InputCommand::KeyboardKey{ Key::R }, InputCommand::InputState::Trigger);

        // デバッグ用カメラ操作有効化/無効化
        ic->RegisterCommand("DebugCameraToggle", InputCommand::KeyboardKey{ Key::F3 }, InputCommand::InputState::Trigger);
#endif
    }
}

} // namespace KashipanEngine
