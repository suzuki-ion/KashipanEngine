#pragma once
#include <KashipanEngine.h>
#include "Scenes/EngineLogoScene.h"
#include "Scenes/TitleScene.h"
#include "Scenes/GameScene.h"
#include "Scenes/ResultScene.h"

namespace KashipanEngine {

inline void AppInitialize(const GameEngine::Context &context) {
    auto *mainWindow = Window::CreateNormal("3104_グランナー", 1280, 720);
#if defined(DEBUG_BUILD) or defined(DEVELOPMENT_BUILD)
    mainWindow->UnregisterWindowEvent(WM_SYSCOMMAND);
    mainWindow->RegisterWindowEvent<WindowDefaultEvent::SysCommandCloseEventSimple>();
#endif

    if (context.sceneManager) {
        auto *sm = context.sceneManager;
        
#if defined(RELEASE_BUILD)
        sm->RegisterScene<EngineLogoScene>("EngineLogoScene", "TitleScene");
#endif
        sm->RegisterScene<TitleScene>("TitleScene");
        sm->RegisterScene<GameScene>("GameScene");
        sm->RegisterScene<ResultScene>("ResultScene");

		context.sceneManager->ChangeScene("TitleScene");
    }

    if (context.inputCommand) {
        auto *ic = context.inputCommand;
        ic->Clear();

        // 決定
        ic->RegisterCommand("Submit", Key::Enter, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Submit", Key::Space, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Submit", ControllerButton::A, InputCommand::InputState::Trigger);
        
        // キャンセル
        ic->RegisterCommand("Cancel", Key::Escape, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Cancel", ControllerButton::B, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Cancel", ControllerButton::Back, InputCommand::InputState::Trigger);

        // タイトル選択（上下）
        ic->RegisterCommand("SelectUp", Key::W, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectUp", Key::Up, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectUp", ControllerButton::DPadUp, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectUp", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Trigger, 0, 0.2f);

        ic->RegisterCommand("SelectDown", Key::S, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectDown", Key::Down, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectDown", ControllerButton::DPadDown, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectDown", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Trigger, 0, 0.2f, true);

        // カメラ後方確認
        ic->RegisterCommand("CameraRearConfirm", Key::E, InputCommand::InputState::Down);
        ic->RegisterCommand("CameraRearConfirm", ControllerButton::X, InputCommand::InputState::Down);

        // プレイヤー移動（左右）
        ic->RegisterCommand("PlayerMoveLeft", Key::A, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveLeft", Key::Left, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveLeft", ControllerButton::DPadLeft, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveLeft", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Down, 0, 0.2f, true);

        ic->RegisterCommand("PlayerMoveRight", Key::D, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveRight", Key::Right, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveRight", ControllerButton::DPadRight, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveRight", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Down, 0, 0.2f);

        // プレイヤージャンプ
        ic->RegisterCommand("PlayerJump", Key::W, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PlayerJump", Key::Up, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PlayerJump", ControllerButton::DPadUp, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PlayerJump", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Trigger, 0, 0.2f);

        // 重力方向切り替え（トリガー/リリース）
        ic->RegisterCommand("PlayerGravitySwitchTrigger", Key::Space, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PlayerGravitySwitchTrigger", ControllerButton::A, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PlayerGravitySwitchRelease", Key::Space, InputCommand::InputState::Release);
        ic->RegisterCommand("PlayerGravitySwitchRelease", ControllerButton::A, InputCommand::InputState::Release);

        // 重力方向入力（上下左右）
        ic->RegisterCommand("PlayerGravityUp", Key::W, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerGravityUp", Key::Up, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerGravityUp", ControllerButton::DPadUp, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerGravityUp", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Down, 0, 0.2f);

        ic->RegisterCommand("PlayerGravityDown", Key::S, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerGravityDown", Key::Down, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerGravityDown", ControllerButton::DPadDown, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerGravityDown", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Down, 0, 0.2f, true);

        ic->RegisterCommand("PlayerGravityLeft", Key::A, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerGravityLeft", Key::Left, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerGravityLeft", ControllerButton::DPadLeft, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerGravityLeft", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Down, 0, 0.2f, true);

        ic->RegisterCommand("PlayerGravityRight", Key::D, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerGravityRight", Key::Right, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerGravityRight", ControllerButton::DPadRight, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerGravityRight", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Down, 0, 0.2f);

#if defined(DEBUG_BUILD) or defined(DEVELOPMENT_BUILD)
        // デバッグ用シーン遷移
        ic->RegisterCommand("DebugSceneChange", Key::F1, InputCommand::InputState::Trigger);
#endif
    }
}

} // namespace KashipanEngine
