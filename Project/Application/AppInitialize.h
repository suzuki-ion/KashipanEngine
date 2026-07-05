#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

inline void AppInitialize(const GameEngine::Context &context) {
    // NOTE: 旧Application層（クラスベースのシーン群）は新シーンシステム（JSONファクトリ方式）への
    //       移行待ちのため、一旦空のデフォルトシーンのみを登録している。
    if (context.sceneManager) {
        auto *sm = context.sceneManager;
        sm->RegisterScene("DefaultScene", LoadJSON("Assets/KashipanEngine/LastSceneBackup/DefaultScene.json"));
        sm->ChangeScene("DefaultScene");
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
        ic->RegisterCommand("SelectDown", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Trigger, 0, -0.2f, true);

        // 選択（左）
        ic->RegisterCommand("SelectLeft", Key::A, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectLeft", Key::Left, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectLeft", ControllerButton::DPadLeft, InputCommand::InputState::Trigger);
        ic->RegisterCommand("SelectLeft", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Trigger, 0, -0.2f, true);

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
        ic->RegisterCommand("PlayerMoveDown", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Down, 0, -0.2f, true);

        // プレイヤー移動（左）
        ic->RegisterCommand("PlayerMoveLeft", Key::A, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveLeft", Key::Left, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveLeft", ControllerButton::DPadLeft, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveLeft", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Down, 0, -0.2f, true);

        // プレイヤー移動（右）
        ic->RegisterCommand("PlayerMoveRight", Key::D, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveRight", Key::Right, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveRight", ControllerButton::DPadRight, InputCommand::InputState::Down);
        ic->RegisterCommand("PlayerMoveRight", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Down, 0, 0.2f);

        // プレイヤージャンプ
        ic->RegisterCommand("PlayerJump", Key::Space, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PlayerJump", ControllerButton::A, InputCommand::InputState::Trigger);

#if defined(DEBUG_BUILD) or defined(DEVELOPMENT_BUILD)
        // デバッグ用シーン遷移
        ic->RegisterCommand("DebugSceneChange", Key::F1, InputCommand::InputState::Trigger);
#endif
    }
}

} // namespace KashipanEngine
