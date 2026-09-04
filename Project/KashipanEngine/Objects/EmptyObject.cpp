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

void EmptyObject::CopyStateFrom(Passkey<Scene>, const EmptyObject &source) {
    SetTag(source.tagName_);
    // このオブジェクトはコンストラクタで既にTransform等をデフォルト構築済みのため、
    // 同型コンポーネントをAddComponentで追加しようとすると最大数チェック（GetMaxComponentCountPerObject）
    // に引っかかって黙って失敗し、複製したデータが破棄されてしまう（Transformなら
    // 常にscale等がデフォルト値のまま残る）。既存の同型コンポーネントは新規追加ではなく
    // 上書きロードで対応する
    std::vector<std::pair<IObjectComponent *, size_t>> existing = components_;
    std::vector<bool> consumed(existing.size(), false);
    for (const auto &comp : source.components_) {
        if (!comp.first) continue;
        auto clonedComp = comp.first->Clone();
        if (!clonedComp) continue;
        // 派生クラスのCloneは基底クラスのタグを複製しないため、ここで引き継ぐ
        clonedComp->SetTag(comp.first->GetTagName());

        size_t typeIndex = clonedComp->GetComponentTypeID();
        IObjectComponent *reused = nullptr;
        for (size_t i = 0; i < existing.size(); ++i) {
            if (consumed[i] || !existing[i].first) continue;
            if (existing[i].first->GetComponentTypeID() != typeIndex) continue;
            reused = existing[i].first;
            consumed[i] = true;
            break;
        }
        if (reused) {
            reused->LoadFromJsonInterface(Passkey<EmptyObject>(), clonedComp->SaveToJsonInterface(Passkey<EmptyObject>()));
        } else {
            AddComponent(std::move(clonedComp));
        }
    }
    SetActive(source.isActive_);
    SetSaveEnabled(source.isSaveEnabled_);
    SetEditorOnly(source.isEditorOnly_);
    SetHiddenFromEditorTarget(source.isHiddenFromEditorTarget_);
}

bool EmptyObject::IsEditorOnlyInHierarchy() const {
    const EmptyObject *current = this;
    while (current) {
        if (current->isEditorOnly_) return true;
        auto *transform = current->GetComponent<Transform>();
        current = transform ? transform->GetParentObject() : nullptr;
    }
    return false;
}

IObjectComponent *EmptyObject::GetComponent(const IObjectComponent *component) const {
    if (component == nullptr) return nullptr;
    auto it = componentsIndexByPointer_.find(component);
    if (it == componentsIndexByPointer_.end()) return nullptr;
    size_t index = it->second;
    if (index >= components_.size()) return nullptr;
    return components_[index].first;
}

IObjectComponent *EmptyObject::GetComponentByAddedID(size_t addedID) const {
    for (const auto &compPair : components_) {
        if (compPair.first && compPair.second == addedID) return compPair.first;
    }
    return nullptr;
}

size_t EmptyObject::HasComponent(const IObjectComponent *component) const {
    if (component == nullptr) return 0;
    auto it = componentsIndexByPointer_.find(component);
    if (it == componentsIndexByPointer_.end()) return 0;
    return 1;
}

IObjectComponent *EmptyObject::RegisterPlacedComponent(IObjectComponent *placed, size_t typeIndex) {
    if (componentsFreeIndices_.size() > 0) {
        size_t freeIndex = componentsFreeIndices_.back();
        componentsFreeIndices_.pop_back();
        if (freeIndex < components_.size()) {
            components_[freeIndex].first = placed;
            components_[freeIndex].second = nextAddedID_++;
            componentsIndexByType_[typeIndex].push_back(freeIndex);
            componentsIndexByPointer_[placed] = freeIndex;
            // オブジェクトが非アクティブの場合は初期化を保留する（有効化時に走る）
            placed->InitializeInterface(Passkey<EmptyObject>(), objectContext_.get(), ownerSceneContext_, isActive_);
            return placed;
        }
    }
    components_.push_back({ placed, nextAddedID_++ });
    componentsIndexByType_[typeIndex].push_back(components_.size() - 1);
    componentsIndexByPointer_[placed] = components_.size() - 1;
    // オブジェクトが非アクティブの場合は初期化を保留する（有効化時に走る）
    placed->InitializeInterface(Passkey<EmptyObject>(), objectContext_.get(), ownerSceneContext_, isActive_);
    return placed;
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
    // comp（渡された一時インスタンス）をそのままプールへムーブすることはしない。
    // ADD_MEMBER_VARIABLE_WITH_CALLBACK等、コンストラクタで自分自身(this)への生ポインタを
    // 登録するリフレクション機構があるため、ムーブするとそれらが古いアドレスを指したまま
    // 破損する。代わりにプールの最終スロットへ直接デフォルト構築し、状態はJSON経由で転送する。
    IComponentPoolBase *pool = ownerSceneContext_ ? ownerSceneContext_->GetOrCreateComponentPool(typeIndex) : nullptr;
    if (!pool) return nullptr;
    IObjectComponent *placed = pool->EmplaceDefault();
    if (!placed) return nullptr;
    placed->LoadFromJsonInterface(Passkey<EmptyObject>(), comp->SaveToJsonInterface(Passkey<EmptyObject>()));
    return RegisterPlacedComponent(placed, typeIndex);
}

IObjectComponent *EmptyObject::AddComponentByTypeID(size_t typeIndex) {
    if (typeIndex >= componentsIndexByType_.size()) {
        componentsIndexByType_.resize(typeIndex + 1);
    }
    IComponentPoolBase *pool = ownerSceneContext_ ? ownerSceneContext_->GetOrCreateComponentPool(typeIndex) : nullptr;
    if (!pool) return nullptr;
    // 引数無しの追加なので、一時インスタンスを経由せずプールの最終スロットへ直接デフォルト構築する
    // （JSON経由の状態転送が不要なため、AddComponent(unique_ptr)より高速）
    IObjectComponent *placed = pool->EmplaceDefault();
    if (!placed) return nullptr;
    if (componentsIndexByType_[typeIndex].size() >= placed->GetMaxComponentCountPerObject()) {
        pool->Remove(placed); // 同じ型のコンポーネントが最大数に達している場合は追加できない
        return nullptr;
    }
    return RegisterPlacedComponent(placed, typeIndex);
}

bool EmptyObject::RemoveComponent(const IObjectComponent *component) {
    if (component == nullptr) return false;
    auto it = componentsIndexByPointer_.find(component);
    if (it == componentsIndexByPointer_.end()) return false;
    size_t index = it->second;
    if (index >= components_.size()) return false;
    IObjectComponent *placed = components_[index].first;
    placed->FinalizeInterface(Passkey<EmptyObject>());
    size_t typeIndex = placed->GetComponentTypeID();
    auto &indices = componentsIndexByType_[typeIndex];
    auto indexIt = std::find(indices.begin(), indices.end(), index);
    if (indexIt != indices.end()) indices.erase(indexIt);
    componentsIndexByPointer_.erase(placed);
    componentsFreeIndices_.push_back(index);
    components_[index].first = nullptr;
    if (ownerSceneContext_) {
        if (IComponentPoolBase *pool = ownerSceneContext_->GetOrCreateComponentPool(typeIndex)) {
            pool->Remove(placed);
        }
    }
    return true;
}

void EmptyObject::ClearComponents() {
    Finalize();
    for (auto &compPair : components_) {
        if (!compPair.first) continue;
        if (ownerSceneContext_) {
            if (IComponentPoolBase *pool = ownerSceneContext_->GetOrCreateComponentPool(compPair.first->GetComponentTypeID())) {
                pool->Remove(compPair.first);
            }
        }
        compPair.first = nullptr;
    }
    components_.clear();
    componentsIndexByType_.clear();
    componentsIndexByPointer_.clear();
    componentsFreeIndices_.clear();
    nextAddedID_ = 0;
}

bool EmptyObject::IsActive() const {
    auto *transform = GetComponent<Transform>();
    // Transform::GetParentObject()（UUID経由）は既に対象オブジェクトの生存確認を済ませて
    // 返すため、ここで改めて objectsExistingSet_ 経由の再確認をする必要はない
    auto *parentObject = transform ? transform->GetParentObject() : nullptr;
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
    for (auto *candidate : ownerSceneContext_->GetSceneObjects()) {
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
    json["editorOnly"] = isEditorOnly_;
    json["hiddenFromEditorTarget"] = isHiddenFromEditorTarget_;
    json["objectID"] = objectID_.ToString();
    if (prefabNodeID_.IsValid()) json["prefabNodeID"] = prefabNodeID_.ToString();

    // updateComponents_ はUpdateループ用のリストで、オブジェクトが非アクティブだと空になるため
    // 保存には使えない（非アクティブなオブジェクトのコンポーネントが消えてしまう）。
    // 保存は実行時の有効状態に関わらず全コンポーネントを対象にする。
    std::vector<const std::pair<IObjectComponent *, size_t> *> ordered;
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
    isEditorOnly_ = json.value("editorOnly", false);
    isHiddenFromEditorTarget_ = json.value("hiddenFromEditorTarget", false);
    objectID_ = UUID128(json.value("objectID", ""));
    prefabNodeID_ = UUID128(json.value("prefabNodeID", ""));
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
        // 先に更新されたコンポーネントが後続コンポーネントを削除する場合があるため、
        // 呼び出し直前に所有状態・追加時ID・アクティブ状態を再確認する。
        const auto it = componentsIndexByPointer_.find(info.component);
        if (it == componentsIndexByPointer_.end() || it->second >= components_.size()) continue;
        auto &[ownedComponent, addedID] = components_[it->second];
        if (!ownedComponent || addedID != info.addedID) continue;
        IObjectComponent *component = ownedComponent;
        if (component && component->IsActive()) {
            component->UpdateInterface(Passkey<EmptyObject>());
        }
    }
}

void EmptyObject::RegenerateUpdateComponentsList() {
    updateComponents_.clear();
    updateComponents_.reserve(components_.size());
    // バッチ処理対象の型が1つも登録されていなければ、型ごとのハッシュ検索そのものを省略する
    // （現時点ではどの型もマークされていないため、毎フレーム・全コンポーネントに対する
    //   無駄なハッシュ検索を避けるための早期リターン）
    const bool hasBatchProcessedTypes = HasAnyBatchProcessedObjectComponentType();
    for (const auto &comp : components_) {
        if (!comp.first || !comp.first->IsActive()) continue;
        // バッチ処理対象としてマークされた型は、個別Updateの対象から除外する
        if (hasBatchProcessedTypes && IsObjectComponentTypeIDBatchProcessed(comp.first->GetComponentTypeID())) continue;
        updateComponents_.push_back({ comp.second, comp.first->GetUpdatePriority(), comp.first });
    }
    // 優先度->追加順の昇順でソート
    std::sort(updateComponents_.begin(), updateComponents_.end(),
        [](const UpdateComponentInfo &a, const UpdateComponentInfo &b) {
            if (a.priority != b.priority) return a.priority < b.priority;
            return a.addedID < b.addedID;
        });
}

} // namespace KashipanEngine
