#include "Scene/SceneBase.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneContext.h"
#include "Scene/Editor/SceneEditorContext.h"
#include "Scene/Components/SceneDefaultVariables.h"
#include "Utilities/FileIO/JSON.h"

#include <algorithm>
#include <cstring>

namespace KashipanEngine {

void SceneBase::SetEnginePointers(
    Passkey<GameEngine>,
    AudioManager *audioManager,
    ModelManager *modelManager,
    SkeletonManager *skeletonManager,
    SamplerManager *samplerManager,
    TextureManager *textureManager,
    AnimationManager *animationManager,
    Input *input,
    InputCommand *inputCommand) {
    sAudioManager = audioManager;
    sModelManager = modelManager;
    sSkeletonManager = skeletonManager;
    sSamplerManager = samplerManager;
    sTextureManager = textureManager;
    sAnimationManager = animationManager;
    sInput = input;
    sInputCommand = inputCommand;
}

SceneBase::SceneBase(const std::string &sceneName)
    : name_(sceneName) {
    sceneContext_ = std::make_unique<SceneContext>(Passkey<SceneBase>{}, this);
#ifdef USE_IMGUI
    sceneEditorContext_ = std::make_unique<SceneEditorContext>(Passkey<SceneBase>{}, this);
    sceneEditor_ = std::make_unique<SceneEditor>(Passkey<SceneBase>{}, sceneEditorContext_.get());
#endif
    // デフォルトのシーン変数コンポーネントを追加
    auto defaultVarsComp = std::make_unique<SceneDefaultVariables>();
    auto *ptr = defaultVarsComp.get();
    AddSceneComponent(std::move(defaultVarsComp));
    ptr->SetSceneComponents([this](std::unique_ptr<ISceneComponent> comp) {
        return AddSceneComponent(std::move(comp));
    });
}

SceneBase::~SceneBase() {
    ClearSceneObjects();
    ClearSceneComponents();
}

void SceneBase::Update() {
    for (auto &o : objects_) {
        if (o) o->Update();
    }

    {
        std::vector<ISceneComponent *> sorted;
        sorted.reserve(sceneComponents_.size());
        for (auto &c : sceneComponents_) {
            if (c) sorted.push_back(c.get());
        }
        std::stable_sort(sorted.begin(), sorted.end(), [](const ISceneComponent *a, const ISceneComponent *b) {
            return a->GetUpdatePriority() < b->GetUpdatePriority();
        });
        for (auto *c : sorted) c->Update();
    }

    OnUpdate();
}

#ifdef USE_IMGUI
void SceneBase::ShowImGui() {
    if (sceneEditor_) {
        sceneEditor_->ShowImGui();
    }
}
#endif

void SceneBase::RebuildObject2DIndices() {
    objects2DIndexByUUID_.clear();
    objects2DIndexByPointer_.clear();
    objects2DIndexByName_.clear();
    for (size_t i = 0; i < objects2D_.size(); ++i) {
        if (!objects2D_[i]) continue;
        objects2DIndexByUUID_.emplace(objects2D_[i]->GetObjectID(), i);
        objects2DIndexByPointer_.emplace(objects2D_[i].get(), i);
        objects2DIndexByName_.emplace(objects2D_[i]->GetName(), i);
    }
}

void SceneBase::RebuildObjectIndices() {
    objects3DIndexByUUID_.clear();
    objects3DIndexByPointer_.clear();
    objects3DIndexByName_.clear();
    for (size_t i = 0; i < objects3D_.size(); ++i) {
        if (!objects3D_[i]) continue;
        objects3DIndexByUUID_.emplace(objects3D_[i]->GetObjectID(), i);
        objects3DIndexByPointer_.emplace(objects3D_[i].get(), i);
        objects3DIndexByName_.emplace(objects3D_[i]->GetName(), i);
    }
}

bool SceneBase::AddObject2D(std::unique_ptr<Object2DBase> obj) {
    if (!obj) return false;
    obj->SetSceneContext(Passkey<SceneBase>(), sceneContext_.get());
    objects2D_.push_back(std::move(obj));

    const size_t idx = objects2D_.size() - 1;
    if (objects2D_[idx]) {
        objects2DIndexByPointer_.emplace(objects2D_[idx].get(), idx);
        objects2DIndexByName_.emplace(objects2D_[idx]->GetName(), idx);
    }
    return true;
}

bool SceneBase::AddObject(std::unique_ptr<EmptyObject> obj) {
    if (!obj) return false;
    obj->SetSceneContext(Passkey<SceneBase>(), sceneContext_.get());
    objects3D_.push_back(std::move(obj));

    const size_t idx = objects3D_.size() - 1;
    if (objects3D_[idx]) {
        objects3DIndexByUUID_.emplace(objects3D_[idx]->GetObjectID(), idx);
        objects3DIndexByPointer_.emplace(objects3D_[idx].get(), idx);
        objects3DIndexByName_.emplace(objects3D_[idx]->GetName(), idx);
    }
    return true;
}

bool SceneBase::InsertObject2D(std::unique_ptr<Object2DBase> obj, size_t index) {
    if (!obj) return false;
    if (index > objects2D_.size()) AddObject2D(std::move(obj));
    else {
        obj->SetSceneContext(Passkey<SceneBase>(), sceneContext_.get());
        objects2D_.insert(objects2D_.begin() + static_cast<std::ptrdiff_t>(index), std::move(obj));
        RebuildObject2DIndices();
    }
    return true;
}

bool SceneBase::InsertObject(std::unique_ptr<EmptyObject> obj, size_t index) {
    if (!obj) return false;
    if (index > objects3D_.size()) AddObject(std::move(obj));
    else {
        obj->SetSceneContext(Passkey<SceneBase>(), sceneContext_.get());
        objects3D_.insert(objects3D_.begin() + static_cast<std::ptrdiff_t>(index), std::move(obj));
        RebuildObjectIndices();
    }
    return true;
}

bool SceneBase::RemoveObject2D(Object2DBase *obj) {
    if (!obj) return false;
    auto it = objects2DIndexByPointer_.find(obj);
    if (it == objects2DIndexByPointer_.end()) return false;
    objects2D_.erase(objects2D_.begin() + static_cast<std::ptrdiff_t>(it->second));
    RebuildObject2DIndices();
    return true;
}

bool SceneBase::RemoveObject(EmptyObject *obj) {
    if (!obj) return false;
    auto it = objects3DIndexByPointer_.find(obj);
    if (it == objects3DIndexByPointer_.end()) return false;
    objects3D_.erase(objects3D_.begin() + static_cast<std::ptrdiff_t>(it->second));
    RebuildObjectIndices();
    return true;
}

bool SceneBase::ReleaseObject2D(Object2DBase *obj) {
    if (!obj) return false;
    auto it = objects2DIndexByPointer_.find(obj);
    if (it == objects2DIndexByPointer_.end()) return false;
    objects2D_[it->second].release();
    objects2D_.erase(objects2D_.begin() + static_cast<std::ptrdiff_t>(it->second));
    RebuildObject2DIndices();
    return true;
}

bool SceneBase::ReleaseObject(EmptyObject *obj) {
    if (!obj) return false;
    auto it = objects3DIndexByPointer_.find(obj);
    if (it == objects3DIndexByPointer_.end()) return false;
    objects3D_[it->second].release();
    objects3D_.erase(objects3D_.begin() + static_cast<std::ptrdiff_t>(it->second));
    RebuildObjectIndices();
    return true;
}

bool SceneBase::MoveObject2D(Object2DBase *obj, size_t newIndex) {
    if (!obj) return false;
    if (newIndex >= objects2D_.size()) newIndex = objects2D_.size() - 1;
    auto it = objects2DIndexByPointer_.find(obj);
    if (it == objects2DIndexByPointer_.end()) return false;
    objects2D_.insert(objects2D_.begin() + static_cast<std::ptrdiff_t>(newIndex), std::move(objects2D_[it->second]));
    if (newIndex < it->second) {
        objects2D_.erase(objects2D_.begin() + static_cast<std::ptrdiff_t>(it->second + 1));
    } else {
        objects2D_.erase(objects2D_.begin() + static_cast<std::ptrdiff_t>(it->second));
    }
    RebuildObject2DIndices();
    return true;
}

bool SceneBase::MoveObject(EmptyObject *obj, size_t newIndex) {
    if (!obj) return false;
    if (newIndex >= objects3D_.size()) newIndex = objects3D_.size() - 1;
    auto it = objects3DIndexByPointer_.find(obj);
    if (it == objects3DIndexByPointer_.end()) return false;
    objects3D_.insert(objects3D_.begin() + static_cast<std::ptrdiff_t>(newIndex), std::move(objects3D_[it->second]));
    if (newIndex < it->second) {
        objects3D_.erase(objects3D_.begin() + static_cast<std::ptrdiff_t>(it->second + 1));
    } else {
        objects3D_.erase(objects3D_.begin() + static_cast<std::ptrdiff_t>(it->second));
    }
    RebuildObjectIndices();
    return true;
}

void SceneBase::ClearObjects2D() {
    objects2D_.clear();
    objects2DIndexByPointer_.clear();
    objects2DIndexByName_.clear();
}

void SceneBase::ClearObjects3D() {
    objects3D_.clear();
    objects3DIndexByUUID_.clear();
    objects3DIndexByPointer_.clear();
    objects3DIndexByName_.clear();
}

std::vector<EmptyObject *> SceneBase::GetSceneObjects(const std::string &objectName) const {
    std::vector<EmptyObject *> objects;
    auto it = objectsIndexByName_.find(objectName);
    if (it == objectsIndexByName_.end()) return objects;
    for (const size_t idx : it->second) {
        if (idx < objects_.size() && objects_[idx]) {
            objects.push_back(objects_[idx].get());
        }
    }
    return objects;
}

EmptyObject *SceneBase::GetSceneObject(const std::string &objectName) const {
    auto it = objectsIndexByName_.find(objectName);
    if (it == objectsIndexByName_.end()) return nullptr;
    for (const size_t idx : it->second) {
        if (idx < objects_.size() && objects_[idx]) {
            return objects_[idx].get();
        }
    }
    return nullptr;
}

EmptyObject *SceneBase::GetSceneObject(EmptyObject *obj) const {
    if (!obj) return nullptr;
    auto it = objectsIndexByPointer_.find(obj);
    if (it == objectsIndexByPointer_.end()) return nullptr;
    return objects_[it->second].get();
}

EmptyObject *SceneBase::GetSceneObject(const UUID128 &uuid) const {
    if (!uuid.IsValid()) return nullptr;
    auto it = objectsIndexByUUID_.find(uuid);
    if (it == objectsIndexByUUID_.end()) return nullptr;
    return objects_[it->second].get();
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
