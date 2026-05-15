#include "Scene/SceneBase.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneContext.h"
#include "Utilities/FileIO/JSON.h"

#include <algorithm>
#include <cstring>

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

namespace KashipanEngine {

namespace {

} // namespace

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

SceneBase::~SceneBase() {
}

void SceneBase::Update() {
    OnUpdate();
}

#if defined(USE_IMGUI)
void SceneBase::ShowImGui() {
}
#endif

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
