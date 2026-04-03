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

		context.sceneManager->ChangeScene("GameScene");
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

#if defined(DEBUG_BUILD) or defined(DEVELOPMENT_BUILD)
        // デバッグ用シーン遷移
        ic->RegisterCommand("DebugSceneChange", Key::F1, InputCommand::InputState::Trigger);
#endif
    }
}

} // namespace KashipanEngine
