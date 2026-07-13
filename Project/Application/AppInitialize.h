#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

inline void AppInitialize(const GameEngine::Context &context) {
    if (context.sceneManager) {
        auto *sm = context.sceneManager;
        sm->RegisterScene("GameScene", LoadJSON("Assets/KashipanEngine/LastSceneBackup/GameScene.json"));
        sm->ChangeScene("GameScene");
    }
}

} // namespace KashipanEngine
