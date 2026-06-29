#include "EmptyObject.h"

namespace KashipanEngine {

EmptyObject::EmptyObject(SceneContext *ownerSceneContext, const std::string &name) {
    objectContext_ = std::make_unique<ObjectContext>(Passkey<EmptyObject>(), this);
    ownerSceneContext_ = ownerSceneContext;
    name_ = name;
}

std::unique_ptr<EmptyObject> EmptyObject::Clone() const {
    std::unique_ptr<EmptyObject> newObj = std::make_unique<EmptyObject>(ownerSceneContext_, name_);
    for (const auto &comp : components_) {
        if (comp.first) {
            auto clonedComp = comp.first->Clone();
            if (clonedComp) {
                newObj->AddComponent(std::move(clonedComp));
            }
        }
    }
    newObj->SetActive(isActive_);
    newObj->SetSaveEnabled(isSaveEnabled_);
    return newObj;
}

void EmptyObject::Update(Passkey<SceneBase>) {
    static std::vector<IObjectComponent *> updateComponents;
    updateComponents.clear();
    updateComponents.reserve(components_.size());
    for (const auto &comp : components_) {
        if (comp && comp->IsActive()) {
            updateComponents.push_back(comp.get());
        }
    }
    std::sort(updateComponents.begin(), updateComponents.end(), [](IObjectComponent *a, IObjectComponent *b) {
        return a->GetUpdatePriority() < b->GetUpdatePriority();
        });
    for (auto *comp : updateComponents) {
        comp->UpdateInterface(Passkey<EmptyObject>());
    }
}

bool EmptyObject::AddComponent(std::unique_ptr<IObjectComponent> comp) {
    if (!comp) return false;
    size_t typeIndex = comp->GetComponentTypeID();
    if (typeIndex >= componentsIndexByType_.size()) {
        componentsIndexByType_.resize(typeIndex + 1);
    }
    if (componentsIndexByType_[typeIndex].size() >= comp->GetMaxComponentCountPerObject()) {
        return false; // 同じ型のコンポーネントが最大数に達している場合は追加できない
    }
    if (componentsFreeIndices_.size() > 0) {
        size_t freeIndex = componentsFreeIndices_.back();
        componentsFreeIndices_.pop_back();
        if (freeIndex < components_.size()) {
            components_[freeIndex] = { std::move(comp), 0 };
            componentsIndexByType_[typeIndex].push_back(freeIndex);
            components_[freeIndex]->InitializeInterface(Passkey<EmptyObject>(), objectContext_.get(), ownerSceneContext_);
            return true;
        }
    }
    components_.push_back({ std::move(comp), 0 });
    componentsIndexByType_[typeIndex].push_back(components_.size() - 1);
    components_.back().first->InitializeInterface(Passkey<EmptyObject>(), objectContext_.get(), ownerSceneContext_);
    return true;
}

bool EmptyObject::RemoveComponent(IObjectComponent *component) {
    if (!component) return false;
    auto it = componentsIndexByPointer_.find(component);
    if (it == componentsIndexByPointer_.end()) return false;
    size_t index = it->second;
    if (index >= components_.size()) return false;
    components_[index].first->FinalizeInterface(Passkey<EmptyObject>());
    components_.erase(components_.begin() + index);
    return true;
}

void EmptyObject::RebuildComponentIndexTables() {
    componentsIndexByType_.clear();
    componentsIndexByPointer_.clear();
    for (size_t i = 0; i < components_.size(); ++i) {
        auto &comp = components_[i];
        if (!comp.first) continue;
        size_t typeIndex = comp.first->GetComponentTypeID();
        if (typeIndex >= componentsIndexByType_.size()) {
            componentsIndexByType_.resize(typeIndex + 1);
        }
        componentsIndexByType_[typeIndex].push_back(i);
        componentsIndexByPointer_[comp.first.get()] = i;
    }
}

} // namespace KashipanEngine