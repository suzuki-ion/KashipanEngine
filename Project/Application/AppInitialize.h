#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

inline void AppInitialize(const GameEngine::Context &context) {
    if (context.sceneManager) {
        auto *sm = context.sceneManager;
        sm->RegisterScene("DefaultScene", LoadJSON("Assets/KashipanEngine/LastSceneBackup/DefaultScene.json"));
        sm->ChangeScene("DefaultScene");
    }
}

} // namespace KashipanEngine
