#pragma once
#include <KashipanEngine.h>
#include "Scenes/EngineLogoScene.h"
#include "Scenes/TitleScene.h"
#include "Scenes/GameScene.h"
#include "Scenes/ResultScene.h"
#if defined(DEBUG_BUILD) or defined(DEVELOPMENT_BUILD)
#include "Scenes/TestScene.h"
#endif

namespace KashipanEngine {

inline void AppInitialize(const GameEngine::Context &context) {
    auto *mainWindow = Window::CreateNormal("Main Window", 1280, 720);
#if defined(DEBUG_BUILD) or defined(DEVELOPMENT_BUILD)
    mainWindow->UnregisterWindowEvent(WM_SYSCOMMAND);
    mainWindow->RegisterWindowEvent<WindowDefaultEvent::SysCommandCloseEventSimple>();
#else
    static_cast<void>(mainWindow); // リリースビルドで未使用の変数警告回避
#endif

    if (context.sceneManager) {
        auto *sm = context.sceneManager;
        
#if defined(RELEASE_BUILD)
        sm->RegisterScene<EngineLogoScene>("EngineLogoScene", "TitleScene");
#endif
#if defined(DEBUG_BUILD) or defined(DEVELOPMENT_BUILD)
        sm->RegisterScene<TestScene>("TestScene");
#endif
        sm->RegisterScene<TitleScene>("TitleScene");
        sm->RegisterScene<GameScene>("GameScene");
        sm->RegisterScene<ResultScene>("ResultScene");

		sm->ChangeScene("TitleScene");
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

        // ポーズ
        ic->RegisterCommand("Pause", Key::Escape, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Pause", Key::P, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Pause", ControllerButton::Start, InputCommand::InputState::Trigger);

        // 選択（上）
        ic->RegisterCommand("SelectUp", Key::W, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectUp", Key::Up, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectUp", ControllerButton::DPadUp, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectUp", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Trigger, 0, 0.2f);

        // 選択（下）
        ic->RegisterCommand("SelectDown", Key::S, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectDown", Key::Down, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectDown", ControllerButton::DPadDown, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectDown", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Trigger, 0, 0.2f, true);

        // 選択（左）
        ic->RegisterCommand("SelectLeft", Key::A, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectLeft", Key::Left, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectLeft", ControllerButton::DPadLeft, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectLeft", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Trigger, 0, -0.2f);

        // 選択（右）
        ic->RegisterCommand("SelectRight", Key::D, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectRight", Key::Right, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectRight", ControllerButton::DPadRight, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectRight", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Trigger, 0, 0.2f);

        // プレイヤー移動（上）
        ic->RegisterCommand("PlayerMoveUp", Key::W, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveUp", Key::Up, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveUp", ControllerButton::DPadUp, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveUp", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Down, 0, 0.2f);

        // プレイヤー移動（下）
        ic->RegisterCommand("PlayerMoveDown", Key::S, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveDown", Key::Down, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveDown", ControllerButton::DPadDown, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveDown", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Down, 0, 0.2f, true);

        // プレイヤー移動（左）
        ic->RegisterCommand("PlayerMoveLeft", Key::A, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveLeft", Key::Left, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveLeft", ControllerButton::DPadLeft, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveLeft", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Down, 0, -0.2f);

        // プレイヤー移動（右）
        ic->RegisterCommand("PlayerMoveRight", Key::D, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveRight", Key::Right, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveRight", ControllerButton::DPadRight, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveRight", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Down, 0, 0.2f);

#if defined(DEBUG_BUILD) or defined(DEVELOPMENT_BUILD)
        // デバッグ用シーン遷移
        ic->RegisterCommand("DebugSceneChange", Key::F1, InputCommand::InputState::Trigger);
#endif
    }
}

} // namespace KashipanEngine
