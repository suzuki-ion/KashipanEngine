#include "Scene/SceneBase.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneContext.h"

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
    sceneContext_ = std::make_unique<SceneContext>(Passkey<SceneBase>{}, this);
}

SceneBase::~SceneBase() {
    ClearSceneComponents();
    ClearObjects2D();
    ClearObjects3D();
}

void SceneBase::Update() {
    for (auto &o : objects2D_) {
        if (o) o->Update();
    }
    for (auto &o : objects3D_) {
        if (o) o->Update();
    }

    {
        std::vector<ISceneComponent *> sorted;
        sorted.reserve(sceneComponents_.size());
        for (auto &c : sceneComponents_) if (c) sorted.push_back(c.get());
        std::stable_sort(sorted.begin(), sorted.end(), [](const ISceneComponent *a, const ISceneComponent *b) {
            return a->GetUpdatePriority() < b->GetUpdatePriority();
        });
        for (auto *c : sorted) c->Update();
    }

    OnUpdate();
}

#if defined(USE_IMGUI)
void SceneBase::ShowImGui() {
    ImGui::Begin("Scene Objects");
    ImGui::TextUnformatted("Scene");
    ImGui::SameLine();
    ImGui::TextUnformatted(name_.c_str());

    if (ImGui::CollapsingHeader("Objects2D", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto &o : objects2D_) {
            if (!o) continue;
            const std::string &objName = o->GetName();
            if (ImGui::TreeNode(objName.c_str())) {
                o->ShowImGui();
                ImGui::TreePop();
            }
        }
    }

    if (ImGui::CollapsingHeader("Objects3D", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto &o : objects3D_) {
            if (!o) continue;
            const std::string &objName = o->GetName();
            if (ImGui::TreeNode(objName.c_str())) {
                o->ShowImGui();
                ImGui::TreePop();
            }
        }
    }
    ImGui::End();

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

bool SceneBase::AddObject2D(std::unique_ptr<Object2DBase> obj) {
    if (!obj) return false;
    objects2D_.push_back(std::move(obj));
    return true;
}

bool SceneBase::AddObject3D(std::unique_ptr<Object3DBase> obj) {
    if (!obj) return false;
    objects3D_.push_back(std::move(obj));
    return true;
}

bool SceneBase::RemoveObject2D(Object2DBase *obj) {
    if (!obj) return false;
    auto it = std::find_if(objects2D_.begin(), objects2D_.end(), [&](const auto &p) { return p.get() == obj; });
    if (it == objects2D_.end()) return false;
    objects2D_.erase(it);
    return true;
}

bool SceneBase::RemoveObject3D(Object3DBase *obj) {
    if (!obj) return false;
    auto it = std::find_if(objects3D_.begin(), objects3D_.end(), [&](const auto &p) { return p.get() == obj; });
    if (it == objects3D_.end()) return false;
    objects3D_.erase(it);
    return true;
}

void SceneBase::ClearObjects2D() {
    objects2D_.clear();
}

void SceneBase::ClearObjects3D() {
    objects3D_.clear();
}

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
