#include "Scene/SceneManager.h"

#include "Scene/Scene.h"

namespace KashipanEngine {

bool SceneManager::ChangeScene(const std::string &sceneName) {
    auto itF = factoriesByName_.find(sceneName);
    if (itF == factoriesByName_.end()) {
        return false;
    }

    currentSceneLegacy_ = nullptr;
    currentScene_ = itF->second(this);
    return (currentScene_ != nullptr);
}

} // namespace KashipanEngine
