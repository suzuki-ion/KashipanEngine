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

        // パズルゲーム用入力（新仕様 / すべて Trigger）
        // ------------------------------------------------------------
        // P1: Horizontal Move
        ic->RegisterCommand("P1PuzzleBlockMoveHorizontal", Key::A, InputCommand::InputState::Trigger, true);
        ic->RegisterCommand("P1PuzzleBlockMoveHorizontal", Key::D, InputCommand::InputState::Trigger);
        ic->RegisterCommand("P1PuzzleBlockMoveHorizontal", Key::Left, InputCommand::InputState::Trigger, true);
        ic->RegisterCommand("P1PuzzleBlockMoveHorizontal", Key::Right, InputCommand::InputState::Trigger);
        ic->RegisterCommand("P1PuzzleBlockMoveHorizontal", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Trigger, 0, 0.5f);
        ic->RegisterCommand("P1PuzzleBlockMoveHorizontal", ControllerButton::DPadLeft, InputCommand::InputState::Trigger, 0, true);
        ic->RegisterCommand("P1PuzzleBlockMoveHorizontal", ControllerButton::DPadRight, InputCommand::InputState::Trigger, 0);

        // P1: Rotation
        ic->RegisterCommand("P1PuzzleBlockRotation", Key::Q, InputCommand::InputState::Trigger, true);
        ic->RegisterCommand("P1PuzzleBlockRotation", Key::E, InputCommand::InputState::Trigger);
        ic->RegisterCommand("P1PuzzleBlockRotation", InputCommand::ControllerAnalog::LeftTrigger, InputCommand::InputState::Trigger, 0, 0.5f, true);
        ic->RegisterCommand("P1PuzzleBlockRotation", InputCommand::ControllerAnalog::RightTrigger, InputCommand::InputState::Trigger, 0, 0.5f);
        ic->RegisterCommand("P1PuzzleBlockRotation", ControllerButton::LeftShoulder, InputCommand::InputState::Trigger, 0, true);
        ic->RegisterCommand("P1PuzzleBlockRotation", ControllerButton::RightShoulder, InputCommand::InputState::Trigger, 0);

        // P1: Place
        ic->RegisterCommand("P1PuzzleBlockPlace", Key::Space, InputCommand::InputState::Trigger);
        ic->RegisterCommand("P1PuzzleBlockPlace", Key::Enter, InputCommand::InputState::Trigger);
        ic->RegisterCommand("P1PuzzleBlockPlace", ControllerButton::A, InputCommand::InputState::Trigger, 0);

        // P2: Horizontal Move (Controller only)
        ic->RegisterCommand("P2PuzzleBlockMoveHorizontal", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Trigger, 1, 0.5f);
        ic->RegisterCommand("P2PuzzleBlockMoveHorizontal", ControllerButton::DPadLeft, InputCommand::InputState::Trigger, 1, true);
        ic->RegisterCommand("P2PuzzleBlockMoveHorizontal", ControllerButton::DPadRight, InputCommand::InputState::Trigger, 1);

        // P2: Rotation (Controller only)
        ic->RegisterCommand("P2PuzzleBlockRotation", InputCommand::ControllerAnalog::LeftTrigger, InputCommand::InputState::Trigger, 1, 0.5f, true);
        ic->RegisterCommand("P2PuzzleBlockRotation", InputCommand::ControllerAnalog::RightTrigger, InputCommand::InputState::Trigger, 1, 0.5f);
        ic->RegisterCommand("P2PuzzleBlockRotation", ControllerButton::LeftShoulder, InputCommand::InputState::Trigger, 1, true);
        ic->RegisterCommand("P2PuzzleBlockRotation", ControllerButton::RightShoulder, InputCommand::InputState::Trigger, 1);

        // P2: Place (Controller only)
        ic->RegisterCommand("P2PuzzleBlockPlace", ControllerButton::A, InputCommand::InputState::Trigger, 1);
    }
}

} // namespace KashipanEngine
