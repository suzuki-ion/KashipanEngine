#include "Scene/SceneManager.h"
#include "Scene/SceneBase.h"

namespace KashipanEngine {

bool SceneManager::ChangeScene(const std::string &sceneName) {
    if (factoriesByName_.find(sceneName) == factoriesByName_.end()) {
        return false;
    }

    pendingSceneName_ = sceneName;
    hasPendingSceneChange_ = true;
    return true;
}

bool SceneManager::CommitPendingSceneChange(Passkey<GameEngine>) {
    if (!hasPendingSceneChange_) {
        return false;
    }

    hasPendingSceneChange_ = false;

    auto itF = factoriesByName_.find(pendingSceneName_);
    if (itF == factoriesByName_.end()) {
        pendingSceneName_.clear();
        return false;
    }

    if (currentScene_) {
        currentScene_->Finalize();
    }

    currentScene_.reset();
    currentScene_ = itF->second(this);

    if (currentScene_) {
        currentScene_->Initialize();
    }

    pendingSceneName_.clear();
    return true;
}

} // namespace KashipanEngine
