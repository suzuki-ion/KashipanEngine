#include "Scene/SceneBase.h"
#include "Scene/SceneManager.h"
#include "Utilities/TimeUtils.h"

namespace KashipanEngine {

void SceneBase::SetEnginePointers(
    Passkey<GameEngine>,
    AudioManager *audioManager,
    ModelManager *modelManager,
    SamplerManager *samplerManager,
    TextureManager *textureManager,
    Input *input,
    InputCommand *inputCommand) {
    sAudioManager = audioManager;
    sModelManager = modelManager;
    sSamplerManager = samplerManager;
    sTextureManager = textureManager;
    sInput = input;
    sInputCommand = inputCommand;
}

SceneBase::SceneBase(const std::string &sceneName)
    : name_(sceneName) {
}

void SceneBase::Update() {
    world_.UpdateAllSystems(GetDeltaTime());
    OnUpdate();
}

void SceneBase::ChangeToNextScene() {
    if (sceneManager_ && !nextSceneName_.empty()) {
        sceneManager_->ChangeScene(nextSceneName_);
    }
}

void SceneBase::AddSceneVariable(const std::string &key, const std::any &value) {
    if (!sceneManager_) return;
    sceneManager_->AddSceneVariable(key, value);
}

const MyStd::AnyUnorderedMap &SceneBase::GetSceneVariables() const {
    static MyStd::AnyUnorderedMap emptyMap;
    if (!sceneManager_) return emptyMap;
    return sceneManager_->GetSceneVariables();
}

} // namespace KashipanEngine
