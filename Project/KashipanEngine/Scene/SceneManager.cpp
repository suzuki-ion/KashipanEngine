#include "Scene/SceneManager.h"

namespace KashipanEngine {
bool SceneManager::ChangeScene(const std::string &sceneName) {
    if (sceneFactoriesData_.find(sceneName) == sceneFactoriesData_.end()) return false;
    pendingSceneName_ = sceneName;
    hasPendingSceneChange_ = true;
    return true;
}

bool SceneManager::RegisterScene(const std::string &sceneName, const JSON &factoryData) {
    auto it = sceneFactoriesData_.find(sceneName);
    if (it != sceneFactoriesData_.end()) return false; // すでに登録されている場合は登録できない
    sceneFactoriesData_[sceneName] = factoryData;
    return true;
}

bool SceneManager::UpdateRegisteredSceneData(const std::string &sceneName, const JSON &factoryData) {
    auto it = sceneFactoriesData_.find(sceneName);
    if (it == sceneFactoriesData_.end()) return false; // 登録されていない場合は更新できない
    it->second = factoryData;
    return true;
}

void SceneManager::Update(Passkey<GameEngine>) {
    if (currentScene_) {
        currentScene_->UpdateInterface(Passkey<SceneManager>());
    }
}

bool SceneManager::CommitPendingSceneChange(Passkey<GameEngine>) {
    if (!hasPendingSceneChange_) return false;
    hasPendingSceneChange_ = false;

    auto itF = sceneFactoriesData_.find(pendingSceneName_);
    if (itF == sceneFactoriesData_.end()) {
        pendingSceneName_.clear();
        return false;
    }

    // 変更前のシーンを終了処理して破棄する
    if (currentScene_) {
        currentScene_->FinalizeInterface(Passkey<SceneManager>());
        currentScene_.reset();
    }

    // 新しいシーンを作成する
    const JSON &factoryData = itF->second;
    // ファクトリデータが空の場合は、シーン名だけでシーンを作成する
    if (factoryData.empty()) {
        currentScene_ = std::make_unique<Scene>(pendingSceneName_);
    } else {
        currentScene_ = std::make_unique<Scene>(factoryData);
    }

    if (currentScene_) {
        currentScene_->SetSceneManager(Passkey<SceneManager>(), this);
        currentScene_->InitializeInterface(Passkey<SceneManager>());
    }

    pendingSceneName_.clear();
    return true;
}

} // namespace KashipanEngine
