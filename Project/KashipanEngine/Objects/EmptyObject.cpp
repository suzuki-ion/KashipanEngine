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
        if (!comp.first) continue;
        auto clonedComp = comp.first->Clone();
        if (!clonedComp) continue;
        newObj->AddComponent(std::move(clonedComp));
    }
    newObj->SetActive(isActive_);
    newObj->SetSaveEnabled(isSaveEnabled_);
    return newObj;
}

void EmptyObject::Update(Passkey<SceneBase>) {
    updateComponents_.clear();
    updateComponents_.reserve(components_.size());
    for (const auto &comp : components_) {
        if (comp.first && comp.first->IsActive()) {
            updateComponents_.push_back({ comp.second, comp.first->GetUpdatePriority(), comp.first.get() });
        }
    }
    // 優先度->追加順の昇順でソート
    std::sort(updateComponents_.begin(), updateComponents_.end(),
        [](const UpdateComponentInfo &a, const UpdateComponentInfo &b) {
            if (a.priority != b.priority) return a.priority < b.priority;
            return a.addedID < b.addedID;
        });
    for (const auto &info : updateComponents_) {
        info.component->UpdateInterface(Passkey<EmptyObject>());
    }
}

IObjectComponent *EmptyObject::GetComponent(const IObjectComponent *component) const {
    if (component == nullptr) return nullptr;
    auto it = componentsIndexByPointer_.find(component);
    if (it == componentsIndexByPointer_.end()) return nullptr;
    size_t index = it->second;
    if (index >= components_.size()) return nullptr;
    return components_[index].first.get();
}

size_t EmptyObject::HasComponent(const IObjectComponent *component) const {
    if (component == nullptr) return 0;
    auto it = componentsIndexByPointer_.find(component);
    if (it == componentsIndexByPointer_.end()) return 0;
    return 1;
}

IObjectComponent *EmptyObject::AddComponent(std::unique_ptr<IObjectComponent> comp) {
    if (!comp) return nullptr;
    size_t typeIndex = comp->GetComponentTypeID();
    if (typeIndex >= componentsIndexByType_.size()) {
        componentsIndexByType_.resize(typeIndex + 1);
    }
    if (componentsIndexByType_[typeIndex].size() >= comp->GetMaxComponentCountPerObject()) {
        return nullptr; // 同じ型のコンポーネントが最大数に達している場合は追加できない
    }
    if (componentsFreeIndices_.size() > 0) {
        size_t freeIndex = componentsFreeIndices_.back();
        componentsFreeIndices_.pop_back();
        if (freeIndex < components_.size()) {
            components_[freeIndex].first = std::move(comp);
            components_[freeIndex].second = nextAddedID_++;
            componentsIndexByType_[typeIndex].push_back(freeIndex);
            components_[freeIndex].first->InitializeInterface(Passkey<EmptyObject>(), objectContext_.get(), ownerSceneContext_);
            return components_[freeIndex].first.get();
        }
    }
    components_.push_back({ std::move(comp), nextAddedID_++ });
    componentsIndexByType_[typeIndex].push_back(components_.size() - 1);
    components_.back().first->InitializeInterface(Passkey<EmptyObject>(), objectContext_.get(), ownerSceneContext_);
    return components_.back().first.get();
}

bool EmptyObject::RemoveComponent(const IObjectComponent *component) {
    if (component == nullptr) return false;
    auto it = componentsIndexByPointer_.find(component);
    if (it == componentsIndexByPointer_.end()) return false;
    size_t index = it->second;
    if (index >= components_.size()) return false;
    components_[index].first->FinalizeInterface(Passkey<EmptyObject>());
    size_t typeIndex = components_[index].first->GetComponentTypeID();
    auto &indices = componentsIndexByType_[typeIndex];
    auto indexIt = std::find(indices.begin(), indices.end(), index);
    if (indexIt != indices.end()) indices.erase(indexIt);
    componentsFreeIndices_.push_back(index);
    components_[index].first.reset();
    return true;
}

} // namespace KashipanEngine