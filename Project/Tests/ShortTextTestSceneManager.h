#pragma once
#include <bit>
#include <cstdint>
#include <memory>
#include "Scene/SceneManager.h"

// Headless short-text regressions construct Scene directly, but global scene variables
// live in SceneManager. Test-only wiring: Passkey is trivially copyable, so tests obtain
// the access tokens with bit_cast instead of booting GameEngine.
// Declare the manager before the Scene so it outlives scene destruction.
namespace KashipanEngine::ShortTextTest {
inline std::unique_ptr<SceneManager> MakeSceneManager() {
    return std::make_unique<SceneManager>(std::bit_cast<Passkey<GameEngine>>(std::uint8_t{}));
}
inline void AttachSceneManager(Scene &scene, SceneManager &manager) {
    scene.SetSceneManager(std::bit_cast<Passkey<SceneManager>>(std::uint8_t{}), &manager);
}
} // namespace KashipanEngine::ShortTextTest
