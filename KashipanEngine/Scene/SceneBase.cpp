#include "Scene/SceneBase.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneContext.h"
#include "Scene/Components/SceneDefaultVariables.h"
#include "Utilities/TimeUtils.h"

#include <algorithm>

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

namespace KashipanEngine {

void SceneBase::SetEnginePointers(
    Passkey<GameEngine>,
    AudioManager *audioManager,
    ModelManager *modelManager,
    SamplerManager *samplerManager,
    TextureManager *textureManager,
    MeshAssets *meshAssets,
    Input *input,
    InputCommand *inputCommand) {
    sAudioManager = audioManager;
    sModelManager = modelManager;
    sSamplerManager = samplerManager;
    sTextureManager = textureManager;
    sMeshAssets = meshAssets;
    sInput = input;
    sInputCommand = inputCommand;
}

SceneBase::SceneBase(const std::string &sceneName)
    : name_(sceneName) {
    sceneContext_ = std::make_unique<SceneContext>(Passkey<SceneBase>{}, this);
    // デフォルトのシーン変数コンポーネントを追加
    auto defaultVarsComp = std::make_unique<SceneDefaultVariables>();
    auto *ptr = defaultVarsComp.get();
    AddSceneComponent(std::move(defaultVarsComp));
    ptr->SetSceneComponents([this](std::unique_ptr<ISceneComponent> comp) {
        return AddSceneComponent(std::move(comp));
    });
}

SceneBase::~SceneBase() {
    ClearSceneComponents();
}

void SceneBase::Update() {
    {
        std::vector<ISceneComponent *> sorted;
        sorted.reserve(sceneComponents_.size());
        for (auto &c : sceneComponents_) if (c) sorted.push_back(c.get());
        std::stable_sort(sorted.begin(), sorted.end(), [](const ISceneComponent *a, const ISceneComponent *b) {
            return a->GetUpdatePriority() < b->GetUpdatePriority();
        });
        for (auto *c : sorted) c->Update();
    }

    world_.UpdateAllSystems(GetDeltaTime());
    OnUpdate();
}

#if defined(USE_IMGUI)
void SceneBase::ShowImGui() {
    ImGui::Begin("Scene Components");
    if (ImGui::CollapsingHeader("Scene Components", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto &c : sceneComponents_) {
            if (!c) continue;
            const std::string &compType = c->GetComponentType();
            if (ImGui::TreeNode(compType.c_str())) {
                c->ShowImGui();
                ImGui::TreePop();
            }
        }
    }
    ImGui::End();
}
#endif

void SceneBase::ChangeToNextScene() {
    if (sceneManager_ && !nextSceneName_.empty()) {
        sceneManager_->ChangeScene(nextSceneName_);
    }
}

bool SceneBase::AddSceneComponent(std::unique_ptr<ISceneComponent> comp) {
    if (!comp) return false;

    const size_t maxCount = comp->GetMaxComponentCountPerScene();
    const size_t existingCount = HasSceneComponents(comp->GetComponentType());
    if (existingCount >= maxCount) return false;

    if (sceneContext_) {
        comp->SetOwnerContext(sceneContext_.get());
    }

    comp->Initialize();

    sceneComponents_.push_back(std::move(comp));

    const size_t idx = sceneComponents_.size() - 1;
    if (sceneComponents_[idx]) {
        sceneComponentsIndexByName_.emplace(sceneComponents_[idx]->GetComponentType(), idx);
        sceneComponentsIndexByType_.emplace(std::type_index(typeid(*sceneComponents_[idx])), idx);
    }

    return true;
}

bool SceneBase::RemoveSceneComponent(ISceneComponent *comp) {
    if (!comp) return false;
    auto it = std::find_if(sceneComponents_.begin(), sceneComponents_.end(), [&](const auto &p) { return p.get() == comp; });
    if (it == sceneComponents_.end()) return false;

    if (*it) {
        (*it)->Finalize();
    }

    sceneComponents_.erase(it);

    sceneComponentsIndexByName_.clear();
    sceneComponentsIndexByType_.clear();
    for (size_t i = 0; i < sceneComponents_.size(); ++i) {
        if (!sceneComponents_[i]) continue;
        sceneComponentsIndexByName_.emplace(sceneComponents_[i]->GetComponentType(), i);
        sceneComponentsIndexByType_.emplace(std::type_index(typeid(*sceneComponents_[i])), i);
    }

    return true;
}

void SceneBase::ClearSceneComponents() {
    for (auto &c : sceneComponents_) {
        if (!c) continue;
        c->Finalize();
    }
    sceneComponents_.clear();
    sceneComponentsIndexByName_.clear();
    sceneComponentsIndexByType_.clear();
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
