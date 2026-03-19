#include "Scene/SceneBase.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneContext.h"
#include "Scene/Components/SceneDefaultVariables.h"

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
    // デフォルトのシーン変数コンポーネントを追加
    auto defaultVarsComp = std::make_unique<SceneDefaultVariables>();
    auto *ptr = defaultVarsComp.get();
    AddSceneComponent(std::move(defaultVarsComp));
    ptr->SetSceneComponents([this](std::unique_ptr<ISceneComponent> comp) {
        return AddSceneComponent(std::move(comp));
    });
}

SceneBase::~SceneBase() {
    ClearObjects2D();
    ClearObjects3D();
    ClearSceneComponents();
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

void SceneBase::RebuildObject2DIndices() {
    objects2DIndexByPointer_.clear();
    objects2DIndexByName_.clear();
    for (size_t i = 0; i < objects2D_.size(); ++i) {
        if (!objects2D_[i]) continue;
        objects2DIndexByPointer_.emplace(objects2D_[i].get(), i);
        objects2DIndexByName_.emplace(objects2D_[i]->GetName(), i);
    }
}

void SceneBase::RebuildObject3DIndices() {
    objects3DIndexByPointer_.clear();
    objects3DIndexByName_.clear();
    for (size_t i = 0; i < objects3D_.size(); ++i) {
        if (!objects3D_[i]) continue;
        objects3DIndexByPointer_.emplace(objects3D_[i].get(), i);
        objects3DIndexByName_.emplace(objects3D_[i]->GetName(), i);
    }
}

bool SceneBase::AddObject2D(std::unique_ptr<Object2DBase> obj) {
    if (!obj) return false;
    objects2D_.push_back(std::move(obj));

    const size_t idx = objects2D_.size() - 1;
    if (objects2D_[idx]) {
        objects2DIndexByPointer_.emplace(objects2D_[idx].get(), idx);
        objects2DIndexByName_.emplace(objects2D_[idx]->GetName(), idx);
    }
    return true;
}

bool SceneBase::AddObject3D(std::unique_ptr<Object3DBase> obj) {
    if (!obj) return false;
    objects3D_.push_back(std::move(obj));

    const size_t idx = objects3D_.size() - 1;
    if (objects3D_[idx]) {
        objects3DIndexByPointer_.emplace(objects3D_[idx].get(), idx);
        objects3DIndexByName_.emplace(objects3D_[idx]->GetName(), idx);
    }
    return true;
}

bool SceneBase::RemoveObject2D(Object2DBase *obj) {
    if (!obj) return false;

    bool found = false;
    auto range = objects2DIndexByPointer_.equal_range(obj);
    for (auto it = range.first; it != range.second; ++it) {
        const size_t idx = it->second;
        if (idx < objects2D_.size() && objects2D_[idx] && objects2D_[idx].get() == obj) {
            objects2D_.erase(objects2D_.begin() + static_cast<std::ptrdiff_t>(idx));
            found = true;
            break;
        }
    }

    if (!found) return false;
    RebuildObject2DIndices();
    return true;
}

bool SceneBase::RemoveObject3D(Object3DBase *obj) {
    if (!obj) return false;

    bool found = false;
    auto range = objects3DIndexByPointer_.equal_range(obj);
    for (auto it = range.first; it != range.second; ++it) {
        const size_t idx = it->second;
        if (idx < objects3D_.size() && objects3D_[idx] && objects3D_[idx].get() == obj) {
            objects3D_.erase(objects3D_.begin() + static_cast<std::ptrdiff_t>(idx));
            found = true;
            break;
        }
    }

    if (!found) return false;
    RebuildObject3DIndices();
    return true;
}

void SceneBase::ClearObjects2D() {
    objects2D_.clear();
    objects2DIndexByPointer_.clear();
    objects2DIndexByName_.clear();
}

void SceneBase::ClearObjects3D() {
    objects3D_.clear();
    objects3DIndexByPointer_.clear();
    objects3DIndexByName_.clear();
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
