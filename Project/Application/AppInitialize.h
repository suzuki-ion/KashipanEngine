#pragma once
#include <KashipanEngine.h>
#include "Scenes/EngineLogoScene.h"
#include "Scenes/TitleScene.h"
#include "Scenes/GameScene.h"
#include "Scenes/ResultScene.h"

namespace KashipanEngine {

inline void AppInitialize(const GameEngine::Context &context) {
    auto monitorInfoOpt = WindowsAPI::QueryMonitorInfo();
    const RECT area = monitorInfoOpt ? monitorInfoOpt->WorkArea() : RECT{ 0, 0, 1280, 720 };

    auto *mainWIndow = Window::CreateNormal("3104_Noisend", 1280, 720);
    mainWIndow;
#if defined(DEBUG_BUILD) or defined(DEVELOPMENT_BUILD)
    mainWIndow->UnregisterWindowEvent(WM_SYSCOMMAND);
    mainWIndow->RegisterWindowEvent<WindowDefaultEvent::SysCommandCloseEventSimple>();
#endif

    if (context.sceneManager) {
        auto *sm = context.sceneManager;
        
        //sm->RegisterScene<EngineLogoScene>("EngineLogoScene", "");
        sm->RegisterScene<TitleScene>("TitleScene");
        sm->RegisterScene<GameScene>("GameScene");
        sm->RegisterScene<ResultScene>("ResultScene");

		context.sceneManager->ChangeScene("TitleScene");
    }

    if (context.inputCommand) {
        auto *ic = context.inputCommand;
        ic->Clear();

        // * ゲーム外のコマンド * //
        // コントローラーのボタン
		ic->RegisterCommand("ControllerA", ControllerButton::A, InputCommand::InputState::Trigger);
		ic->RegisterCommand("ControllerB", ControllerButton::B, InputCommand::InputState::Trigger);
		ic->RegisterCommand("ControllerX", ControllerButton::X, InputCommand::InputState::Trigger);
		ic->RegisterCommand("ControllerY", ControllerButton::Y, InputCommand::InputState::Trigger);
		ic->RegisterCommand("ControllerStart", ControllerButton::Start, InputCommand::InputState::Trigger);
		ic->RegisterCommand("ControllerBack", ControllerButton::Back, InputCommand::InputState::Trigger);
		ic->RegisterCommand("ControllerDPadUp", ControllerButton::DPadUp, InputCommand::InputState::Trigger);
		ic->RegisterCommand("ControllerDPadDown", ControllerButton::DPadDown, InputCommand::InputState::Trigger);
		ic->RegisterCommand("ControllerDPadLeft", ControllerButton::DPadLeft, InputCommand::InputState::Trigger);
		ic->RegisterCommand("ControllerDPadRight", ControllerButton::DPadRight, InputCommand::InputState::Trigger);
		ic->RegisterCommand("ControllerLeftShoulder", ControllerButton::LeftShoulder, InputCommand::InputState::Trigger);
		ic->RegisterCommand("ControllerRightShoulder", ControllerButton::RightShoulder, InputCommand::InputState::Trigger);
        
        // 決定
        ic->RegisterCommand("Submit", Key::Enter, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Submit", Key::Space, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Submit", ControllerButton::A, InputCommand::InputState::Trigger);

        ic->RegisterCommand("P2Submit", ControllerButton::A, InputCommand::InputState::Down, 1);
        
        // キャンセル
        ic->RegisterCommand("Cancel", Key::Escape, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Cancel", ControllerButton::B, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Cancel", ControllerButton::Back, InputCommand::InputState::Trigger);

        ic->RegisterCommand("P2Cancel", ControllerButton::B, InputCommand::InputState::Down, 1);

		// メニュー呼び出し
		ic->RegisterCommand("Menu", Key::M, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Menu", Key::Escape, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Menu", ControllerButton::Start, InputCommand::InputState::Trigger);

		// 上下選択
        ic->RegisterCommand("Up", Key::D, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Up", Key::W, InputCommand::InputState::Trigger);
		ic->RegisterCommand("Up", Key::Up, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Up", Key::Right, InputCommand::InputState::Trigger);
		ic->RegisterCommand("Up", ControllerButton::DPadUp, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Up", ControllerButton::DPadRight, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Down", Key::A, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Down", Key::Left, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Down", Key::S, InputCommand::InputState::Trigger);
		ic->RegisterCommand("Down", Key::Down, InputCommand::InputState::Trigger);
		ic->RegisterCommand("Down", ControllerButton::DPadDown, InputCommand::InputState::Trigger);
		ic->RegisterCommand("Down", ControllerButton::DPadLeft, InputCommand::InputState::Trigger);

        // デバッグ用シーン遷移
        ic->RegisterCommand("DebugSceneChange", Key::F1, InputCommand::InputState::Trigger);

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

        // パズル時間スキップ
        ic->RegisterCommand("PuzzleTimeSkip", Key::LeftShift, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleTimeSkip", Key::RightShift, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleTimeSkip", ControllerButton::Y, InputCommand::InputState::Trigger);

        // パズルステージ切り替え
        ic->RegisterCommand("PuzzleSwitchBoard", Key::E, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleSwitchBoard", Key::Enter, InputCommand::InputState::Trigger);
        ic->RegisterCommand("PuzzleSwitchBoard", InputCommand::ControllerAnalog::LeftTrigger, InputCommand::InputState::Trigger, 0, 0.5f);
        ic->RegisterCommand("PuzzleSwitchBoard", InputCommand::ControllerAnalog::RightTrigger, InputCommand::InputState::Trigger, 0, 0.5f);

        // パズルゲーム用入力（2P用：コントローラー0）
        // コントローラー1台の場合：1P=キーボード、2P=コントローラー0
        // コントローラー2台の場合：1P=コントローラー0、2P=コントローラー1
        // ※ 2P用コマンドはP2Puzzle*という名前で登録
        ic->RegisterCommand("P2PuzzleUp", ControllerButton::DPadUp, InputCommand::InputState::Trigger, 1);
        ic->RegisterCommand("P2PuzzleUp", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Trigger, 1, 0.5f);

        ic->RegisterCommand("P2PuzzleDown", ControllerButton::DPadDown, InputCommand::InputState::Trigger, 1);
        ic->RegisterCommand("P2PuzzleDown", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Trigger, 1, -0.5f);

        ic->RegisterCommand("P2PuzzleLeft", ControllerButton::DPadLeft, InputCommand::InputState::Trigger, 1);
        ic->RegisterCommand("P2PuzzleLeft", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Trigger, 1, -0.5f);

        ic->RegisterCommand("P2PuzzleRight", ControllerButton::DPadRight, InputCommand::InputState::Trigger, 1);
        ic->RegisterCommand("P2PuzzleRight", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Trigger, 1, 0.5f);

        ic->RegisterCommand("P2PuzzleActionHold", ControllerButton::A, InputCommand::InputState::Down, 1);

        ic->RegisterCommand("P2PuzzleTimeSkip", ControllerButton::Y, InputCommand::InputState::Trigger, 1);

        ic->RegisterCommand("P2PuzzleSwitchBoard", InputCommand::ControllerAnalog::LeftTrigger, InputCommand::InputState::Trigger, 1, 0.5f);
        ic->RegisterCommand("P2PuzzleSwitchBoard", InputCommand::ControllerAnalog::RightTrigger, InputCommand::InputState::Trigger, 1, 0.5f);
    }
}

} // namespace KashipanEngine
