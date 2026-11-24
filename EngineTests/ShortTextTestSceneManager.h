#pragma once
#include <bit>
#include <cstdint>
#include <memory>
#include "Scene/SceneManager.h"

// ウィンドウを使わない短文テストは Scene を直接構築する。一方、シーンのグローバル変数は
// SceneManager が持つ。Passkey はトリビアルにコピー可能なので、テストでは GameEngine を起動せず、
// bit_cast でアクセストークンを取得して両者を接続する。
// マネージャーを Scene より先に宣言し、シーンの破棄後まで生存させる。
namespace KashipanEngine::ShortTextTest {
inline std::unique_ptr<SceneManager> MakeSceneManager() {
    return std::make_unique<SceneManager>(std::bit_cast<Passkey<GameEngine>>(std::uint8_t{}));
}
inline void AttachSceneManager(Scene &scene, SceneManager &manager) {
    scene.SetSceneManager(std::bit_cast<Passkey<SceneManager>>(std::uint8_t{}), &manager);
}
} // namespace KashipanEngine::ShortTextTest
