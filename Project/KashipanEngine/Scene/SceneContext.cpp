#include "Scene/SceneContext.h"

namespace KashipanEngine {

const std::string &SceneContext::GetName() const {
    static const std::string kEmpty;
    if (!owner_) return kEmpty;
    return owner_->GetName();
}

std::vector<ISceneComponent *> SceneContext::GetComponents(const std::string &componentName) const {
    if (componentName.empty() || !owner_) return {};
    return owner_->GetSceneComponents(componentName);
}

ISceneComponent *SceneContext::GetComponent(const std::string &componentName) const {
    if (componentName.empty() || !owner_) return nullptr;
    return owner_->GetSceneComponent(componentName);
}

size_t SceneContext::HasComponents(const std::string &componentName) const {
    if (componentName.empty() || !owner_) return 0;
    return owner_->HasSceneComponents(componentName);
}

void SceneContext::AddSceneVariable(const std::string &key, const std::any &value) {
    if (key.empty() || !owner_) return;
    owner_->AddSceneVariable(key, value);
}

const MyStd::AnyUnorderedMap &SceneContext::GetSceneVariables() const {
    static const MyStd::AnyUnorderedMap kEmpty;
    if (!owner_) return kEmpty;
    return owner_->GetSceneVariables();
}

bool SceneContext::AddObject2D(std::unique_ptr<Object2DBase> obj) {
    if (!owner_) return false;
    return owner_->AddObject2D(std::move(obj));
}

bool SceneContext::AddObject3D(std::unique_ptr<Object3DBase> obj) {
    if (!owner_) return false;
    return owner_->AddObject3D(std::move(obj));
}

bool SceneContext::RemoveObject2D(Object2DBase *obj) {
    if (!owner_) return false;
    return owner_->RemoveObject2D(obj);
}

bool SceneContext::RemoveObject3D(Object3DBase *obj) {
    if (!owner_) return false;
    return owner_->RemoveObject3D(obj);
}

std::vector<Object2DBase *> SceneContext::GetObjects2D(const std::string &objectName) const {
    if (objectName.empty() || !owner_) return {};
    return owner_->GetObjects2D(objectName);
}

Object2DBase *SceneContext::GetObject2D(const std::string &objectName) const {
    if (objectName.empty() || !owner_) return nullptr;
    return owner_->GetObject2D(objectName);
}

Object2DBase *SceneContext::GetObject2D(Object2DBase *obj) const {
    if (!obj || !owner_) return nullptr;
    return owner_->GetObject2D(obj);
}

std::vector<Object3DBase *> SceneContext::GetObjects3D(const std::string &objectName) const {
    if (objectName.empty() || !owner_) return {};
    return owner_->GetObjects3D(objectName);
}

Object3DBase *SceneContext::GetObject3D(const std::string &objectName) const {
    if (objectName.empty() || !owner_) return nullptr;
    return owner_->GetObject3D(objectName);
}

Object3DBase *SceneContext::GetObject3D(Object3DBase *obj) const {
    if (!obj || !owner_) return nullptr;
    return owner_->GetObject3D(obj);
}

} // namespace KashipanEngine
