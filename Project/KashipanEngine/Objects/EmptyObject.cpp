#include "EmptyObject.h"
#include "Objects/Components/Transform.h"
#include "Scene/SceneContext.h"

namespace KashipanEngine {

EmptyObject::EmptyObject(SceneContext *ownerSceneContext, const std::string &name) {
    objectContext_ = std::make_unique<ObjectContext>(Passkey<EmptyObject>(), this);
    ownerSceneContext_ = ownerSceneContext;
    name_ = name;
    AddComponent(std::make_unique<Transform>());
}

EmptyObject::~EmptyObject() {
    ClearComponents();
}

std::unique_ptr<EmptyObject> EmptyObject::Clone() const {
    std::unique_ptr<EmptyObject> newObj(new EmptyObject(ownerSceneContext_, name_));
    newObj->SetTag(tagName_);
    for (const auto &comp : components_) {
        if (!comp.first) continue;
        auto clonedComp = comp.first->Clone();
        if (!clonedComp) continue;
        // 派生クラスのCloneは基底クラスのタグを複製しないため、ここで引き継ぐ
        clonedComp->SetTag(comp.first->GetTagName());
        newObj->AddComponent(std::move(clonedComp));
    }
    newObj->SetActive(isActive_);
    newObj->SetSaveEnabled(isSaveEnabled_);
    return newObj;
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
            componentsIndexByPointer_[components_[freeIndex].first.get()] = freeIndex;
            // オブジェクトが非アクティブの場合は初期化を保留する（有効化時に走る）
            components_[freeIndex].first->InitializeInterface(Passkey<EmptyObject>(), objectContext_.get(), ownerSceneContext_, isActive_);
            return components_[freeIndex].first.get();
        }
    }
    components_.push_back({ std::move(comp), nextAddedID_++ });
    componentsIndexByType_[typeIndex].push_back(components_.size() - 1);
    componentsIndexByPointer_[components_.back().first.get()] = components_.size() - 1;
    // オブジェクトが非アクティブの場合は初期化を保留する（有効化時に走る）
    components_.back().first->InitializeInterface(Passkey<EmptyObject>(), objectContext_.get(), ownerSceneContext_, isActive_);
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
    componentsIndexByPointer_.erase(components_[index].first.get());
    componentsFreeIndices_.push_back(index);
    components_[index].first.reset();
    return true;
}

void EmptyObject::ClearComponents() {
    Finalize();
    for (auto &compPair : components_) {
        compPair.first.reset();
    }
    components_.clear();
    componentsIndexByType_.clear();
    componentsIndexByPointer_.clear();
    componentsFreeIndices_.clear();
    nextAddedID_ = 0;
}

bool EmptyObject::IsActive() const {
    auto *transform = GetComponent<Transform>();
    auto *parentObject = transform ? transform->GetParentObject() : nullptr;
    parentObject = ownerSceneContext_ ? ownerSceneContext_->GetSceneObject(parentObject) : nullptr;
    bool parentActive = parentObject ? parentObject->IsActive() : true;
    return isActive_ && parentActive;
}

void EmptyObject::SetActive(bool active) {
    if (isActive_ == active) return;

    // 実効アクティブ状態（IsActive）の変化を検知するため、変更前の子孫の状態を記録しておく
    std::vector<std::pair<EmptyObject *, bool>> descendantsBefore;
    CollectDescendantsActiveState(descendantsBefore);

    isActive_ = active;
    if (IsActive()) {
        Initialize();
    } else {
        Finalize();
    }

    // 実効アクティブ状態が変化した子孫だけInitialize/Finalizeを実行する
    // （子孫自身の isActive フラグは変更しない）
    for (auto &[descendant, wasActive] : descendantsBefore) {
        if (!descendant) continue;
        const bool isActiveNow = descendant->IsActive();
        if (isActiveNow == wasActive) continue;
        if (isActiveNow) {
            descendant->Initialize();
        } else {
            descendant->Finalize();
        }
    }
}

void EmptyObject::CollectDescendantsActiveState(std::vector<std::pair<EmptyObject *, bool>> &out) const {
    if (!ownerSceneContext_) return;
    for (const auto &objPtr : ownerSceneContext_->GetSceneObjects()) {
        EmptyObject *candidate = objPtr.get();
        if (!candidate || candidate == this) continue;

        // candidate が this の子孫かどうかを親チェーンをたどって判定する
        auto *transform = candidate->GetComponent<Transform>();
        EmptyObject *parent = transform ? transform->GetParentObject() : nullptr;
        bool isDescendant = false;
        while (parent) {
            if (parent == this) {
                isDescendant = true;
                break;
            }
            auto *parentTransform = parent->GetComponent<Transform>();
            parent = parentTransform ? parentTransform->GetParentObject() : nullptr;
        }
        if (!isDescendant) continue;

        out.emplace_back(candidate, candidate->IsActive());
    }
}

JSON EmptyObject::SaveToJson(Passkey<Scene>) {
    JSON json = JSON::object();
    if (!isSaveEnabled_) return json;
    json["name"] = name_;
    json["tag"] = tagName_;
    json["isActive"] = isActive_;
    json["objectID"] = objectID_.ToString();

    // updateComponents_ はUpdateループ用のリストで、オブジェクトが非アクティブだと空になるため
    // 保存には使えない（非アクティブなオブジェクトのコンポーネントが消えてしまう）。
    // 保存は実行時の有効状態に関わらず全コンポーネントを対象にする。
    std::vector<const std::pair<std::unique_ptr<IObjectComponent>, size_t> *> ordered;
    ordered.reserve(components_.size());
    for (const auto &compPair : components_) {
        if (compPair.first) ordered.push_back(&compPair);
    }
    std::sort(ordered.begin(), ordered.end(),
        [](const auto *a, const auto *b) {
            if (a->first->GetUpdatePriority() != b->first->GetUpdatePriority()) {
                return a->first->GetUpdatePriority() < b->first->GetUpdatePriority();
            }
            return a->second < b->second;
        });
    for (const auto *compPair : ordered) {
        JSON compJson;
        compJson["type"] = compPair->first->GetComponentType();
        compJson["data"] = compPair->first->SaveToJsonInterface(Passkey<EmptyObject>());
        json["components"].push_back(compJson);
    }
    return json;
}

bool EmptyObject::LoadFromJson(Passkey<Scene>, const JSON &json) {
    ClearComponents();
    name_ = json.value("name", "EmptyObject");
    SetTag(json.value("tag", std::string{}));
    isActive_ = json.value("isActive", true);
    objectID_ = UUID128(json.value("objectID", ""));
    const auto &componentsJson = json.value("components", JSON::array());
    std::vector<std::pair<IObjectComponent *, JSON>> loadedComponents;
    // 先にコンポーネントを全て登録してからロードする
    for (const auto &compJson : componentsJson) {
        std::string typeName = compJson.value("type", "");
        if (typeName.empty()) continue;
        auto comp = CreateObjectComponentByType(typeName);
        // 追加時の初期化前にアクティブ状態を反映し、無効なコンポーネントの初期化を防ぐ
        if (comp && compJson.contains("data")) {
            comp->SetActive(compJson["data"].value("isActive", true));
        }
        loadedComponents.emplace_back(AddComponent(std::move(comp)), compJson["data"]);
    }
    // 各コンポーネントにJSONデータをロードさせる
    for (const auto &compPair : loadedComponents) {
        IObjectComponent *comp = compPair.first;
        const JSON &compJson = compPair.second;
        if (!comp) continue;
        comp->LoadFromJsonInterface(Passkey<EmptyObject>(), compJson);
    }
    return true;
}

void EmptyObject::Initialize() {
    // アクティブなコンポーネントを優先度順に初期化する
    RegenerateUpdateComponentsList();
    for (auto &compPair : updateComponents_) {
        if (compPair.component) {
            compPair.component->InitializeInterface(Passkey<EmptyObject>(), objectContext_.get(), ownerSceneContext_);
        }
    }
    // 非アクティブなコンポーネントにもコンテキストは設定する（初期化は有効化時に走る）
    for (auto &compPair : components_) {
        if (compPair.first && !compPair.first->IsActive()) {
            compPair.first->InitializeInterface(Passkey<EmptyObject>(), objectContext_.get(), ownerSceneContext_, false);
        }
    }
}

void EmptyObject::Finalize() {
    RegenerateUpdateComponentsList();
    for (auto &compPair : updateComponents_) {
        if (compPair.component) {
            compPair.component->FinalizeInterface(Passkey<EmptyObject>());
        }
    }
}

void EmptyObject::Update() {
    RegenerateUpdateComponentsList();
    for (const auto &info : updateComponents_) {
        info.component->UpdateInterface(Passkey<EmptyObject>());
    }
}

void EmptyObject::RegenerateUpdateComponentsList() {
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
}

} // namespace KashipanEngine
