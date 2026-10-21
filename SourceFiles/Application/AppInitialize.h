#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

inline void AppInitialize(const GameEngine::Context &context) {
    // シーンの定義は開いているプロジェクトの Assets/SceneList.json で行う
    // （シーンエディターの Scene List ウィンドウからも登録・編集・切り替えが可能）。
    // 必要であればここで context.sceneManager->RegisterScene(...) による
    // コード側でのシーン登録・切り替えも引き続き利用できる。
    (void)context;
}

} // namespace KashipanEngine
