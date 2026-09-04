#include "Scene/Scene.h"
#include "Scene/SceneBackupPath.h"
#include "Core/GameEngine.h"
#include "Graphics/GraphicsEngine.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneContext.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Assets/SkeletonManager.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Collider/RigidBody2D.h"
#include "Objects/Components/Collider/RigidBody3D.h"
#include "Debug/Logger.h"
#ifdef USE_IMGUI
#include "Objects/Components/ScriptComponent.h"
#include "Scene/SceneEditor.h"
#include "Scene/SceneEditorContext.h"
#endif

#include <algorithm>
#include <cstring>

namespace KashipanEngine {

void Scene::SetEnginePointers(
    Passkey<GameEngine>,
    AudioManager *audioManager,
    ModelManager *modelManager,
    SkeletonManager *skeletonManager,
    SamplerManager *samplerManager,
    TextureManager *textureManager,
    AnimationManager *animationManager,
    MaterialManager *materialManager,
    Input *input,
    InputCommand *inputCommand) {
    LogScope scope;
    sAudioManager = audioManager;
    sModelManager = modelManager;
    sSkeletonManager = skeletonManager;
    sSamplerManager = samplerManager;
    sTextureManager = textureManager;
    sAnimationManager = animationManager;
    sMaterialManager = materialManager;
    sInput = input;
    sInputCommand = inputCommand;
}

void Scene::RequestExitGameLoop() {
    LogScope scope;
    GameEngine::RequestExitGameLoop();
}

Scene::Scene(const std::string &sceneName)
    : name_(sceneName) {
    LogScope scope;
    sceneContext_ = std::make_unique<SceneContext>(Passkey<Scene>{}, this);
#ifdef USE_IMGUI
    sceneEditorContext_ = std::make_unique<SceneEditorContext>(Passkey<Scene>{}, this);
    sceneEditor_ = std::make_unique<SceneEditor>(Passkey<Scene>{}, sceneEditorContext_.get());
#endif
}

Scene::Scene(const JSON &sceneData) : Scene(std::string("Unnamed Scene")) {
    LogScope scope;
    LoadFromJSON(sceneData);
}

Scene::~Scene() {
    LogScope scope;
#if !defined(RELEASE_BUILD)
    // Releaseビルドではデバッグ用のバックアップ書き出しを行わない
    json sceneData = SaveToJSON();
    std::string filePath = std::string(kSceneBackupDirectory) + name_ + ".json";
    SaveJSON(sceneData, filePath);
#endif
    ClearSceneObjects();
    ClearSceneComponents();
}

#ifdef USE_IMGUI
void Scene::ShowImGuiInterface(Passkey<SceneManager>) {
    LogScope scope;
    if (sceneEditor_) {
        sceneEditor_->ShowImGui();
    }
    // ビューアウィンドウ等、ポーズ中も表示し続けたいコンポーネントのImGui表示
    for (const auto &object : objects_) {
        if (object) {
            object->ShowPersistentImGui(Passkey<Scene>{});
        }
    }
}

void Scene::PlayStart() {
    LogScope scope;
    if (isPlaying_) return;
    // DeleteEditorOnlyObjects以降で大量のGPUリソース（ScreenBuffer等）を即座に破棄する。
    // 通常のフレームループは毎フレーム終端でGPU同期しているため安全だが、Play/Stopは
    // GameLoopUpdate()の途中（＝直前フレームの描画がGPU側で完了しているとは限らないタイミング）
    // で割り込むため、ここで明示的に同期してから破棄する（未完了のまま破棄するとGPUハング/
    // スワップチェーンPresent失敗を引き起こしうる。Play/Stopの高速連打で再現するクラッシュの対策）
    if (sDirectXCommon_) sDirectXCommon_->WaitForGPUIdle(Passkey<Scene>{});
    editModeSnapshot_ = SaveToJSON();

    // EditorOnlyオブジェクトは再生中のシーンには存在させない（子孫ごと削除される）。
    // スナップショットには保存済みのため、PlayStopでの復元時に元へ戻る
    DeleteEditorOnlyObjects();

    // 物理ボディは生成された時点の位置のまま追従しないため、エディターでの移動を反映してから再生を開始する
    // （反映しないと、生成時点の古い位置へUpdateで引き戻されてしまう）
    for (const auto &object : objects_) {
        if (!object) continue;
        for (auto *rigidBody : object->GetComponents<RigidBody3D>()) {
            rigidBody->SyncFromTransform();
        }
        for (auto *rigidBody2D : object->GetComponents<RigidBody2D>()) {
            rigidBody2D->SyncFromTransform();
        }
        // ScriptComponentが参照する.asファイルは、通常はコンポーネント追加時（Initialize）にしか
        // 読み込まれない。エディター上で編集した内容を再生開始時点で反映できるよう、
        // Initializeと同様の手順（Reload→フック張り直し→Awake）で読み直す
        for (auto *script : object->GetComponents<ScriptComponent>()) {
            script->ReloadFromDisk();
        }
    }

    isPlaying_ = true;
    isPaused_ = false;
    isStepFrameRequested_ = false;
}

void Scene::PlayStop() {
    LogScope scope;
    if (!isPlaying_) return;
    // PlayStart側と同じ理由。ClearSceneObjects/ClearSceneComponentsで大量のGPUリソースを
    // 即座に破棄する前に、直前フレームのGPU処理が確実に完了していることを保証する
    if (sDirectXCommon_) sDirectXCommon_->WaitForGPUIdle(Passkey<Scene>{});
    isPlaying_ = false;
    isPaused_ = false;
    isStepFrameRequested_ = false;

    // SkinnedMeshRendererのアニメーションは各コンポーネントが専用に複製したスケルトンインスタンスの
    // ジョイントTransformを直接書き換えて進行するため、シーンオブジェクトを再生開始前の状態へ
    // 戻すだけでは元のポーズに戻らない。ここで明示的にバインドポーズへ復元する。
    if (auto *sceneRenderer = GetComponent<SceneRenderer>()) {
        sceneRenderer->ResetAllSkinnedMeshRendererPoses();
    }
    // SkeletonManagerが保持する共有アセット本体（KeyframeAnimator等、複製を使わない別経路の
    // 消費者向け）のジョイント姿勢もバインドポーズへ復元しておく。
    SkeletonManager::ResetAllSkeletonsToBindPose();

    JSON snapshot = std::move(editModeSnapshot_);
    editModeSnapshot_ = JSON();
    if (snapshot.empty()) return;

    ClearSceneObjects();
    ClearSceneComponents();

    // これから読み込む新しいコンポーネント群は、削除された旧インスタンスとは別のアドレス・
    // addedIDを持つ。Renderer::resourceContainer_内のキャッシュ（構造化バッファ等）は
    // インスタンス固有の値をキーへ含むものがあり、ここで破棄しないと旧インスタンス由来の
    // エントリが二度と参照されないまま溜まり続け、再生・停止を繰り返すたびにディスクリプタ
    // ヒープを消費し尽くしてクラッシュする（通常のシーン切り替え時はSceneManagerが
    // 同様の破棄を行うが、Play/Stopはそこを経由しないため漏れていた）
    if (sGraphicsEngine_) sGraphicsEngine_->ReleaseRendererResources(Passkey<Scene>{});

    LoadFromJSON(snapshot);
}
#endif

JSON Scene::SaveToJSON() const {
    LogScope scope;
    JSON json;
    json["sceneName"] = name_;
    json["sceneID"] = sceneID_.ToString();
    for (const auto &compPair : components_) {
        if (!compPair.first) continue;
        JSON compJson;
        compJson["type"] = compPair.first->GetComponentType();
        compJson["data"] = compPair.first->SaveToJsonInterface(Passkey<Scene>{});
        json["sceneComponents"].push_back(compJson);
    }
    for (const auto &obj : objects_) {
        if (!obj || !obj->IsSaveEnabled()) continue;
        json["sceneObjects"].push_back(obj->SaveToJson(Passkey<Scene>{}));
    }
    for (const auto &varPair : sceneVariables_) {
        JSON varJson;
        varJson["key"] = varPair.first;
        varJson["type"] = varPair.second.GetTypeInfo().ToString();
        varJson["value"] = SaveAnyToJson(varPair.second);
        json["sceneVariables"].push_back(varJson);
    }
    return json;
}

bool Scene::LoadFromJSON(const JSON &json) {
    LogScope scope;
    if (json.empty()) return false;
    name_ = json.value("sceneName", "");
    sceneID_ = UUID128(json.value("sceneID", ""));
    // シーンコンポーネントを追加
    std::vector<std::pair<ISceneComponent *, JSON>> loadedComponents;
    // 先にコンポーネントを全て登録してからロードする
    for (const auto &compData : json.value("sceneComponents", std::vector<JSON>())) {
        std::string compType = compData.value("type", "");
        if (compType.empty()) continue;
        auto comp = CreateSceneComponentByType(compType);
        if (!comp) continue;
        auto compJson = compData.value("data", JSON());
        // 追加時の初期化前にアクティブ状態を反映し、無効なコンポーネントの初期化を防ぐ
        comp->SetActive(compJson.value("isActive", true));
        loadedComponents.emplace_back(AddComponent(std::move(comp)), compJson);
    }
    for (const auto &[comp, compJson] : loadedComponents) {
        if (comp) {
            comp->LoadFromJsonInterface(Passkey<Scene>{}, compJson);
        }
    }
    // オブジェクトを全て追加してからオブジェクトにコンポーネントを追加する
    std::vector<EmptyObject *> createdObjects;
    const auto &objects = json.value("sceneObjects", std::vector<JSON>());
    for (const auto &objData : objects) {
        std::string objName = objData.value("name", "Empty Object");
        UUID128 objID(objData.value("objectID", ""));
        EmptyObject *obj = CreateEmptyObject(objName, objID);
        createdObjects.push_back(obj);
    }
    for (size_t i = 0; i < objects.size(); ++i) {
        const auto &objData = objects[i];
        EmptyObject *obj = createdObjects[i];
        obj->LoadFromJson(Passkey<Scene>{}, objData);
    }
#if !defined(USE_IMGUI)
    // エディター無しビルドではEditorOnlyオブジェクトをシーンに存在させない
    // （エディターありの場合は再生開始時（PlayStart）に削除される）
    DeleteEditorOnlyObjects();
#endif
    // シーン変数を追加
    for (const auto &varData : json.value("sceneVariables", std::vector<JSON>())) {
        std::string key = varData.value("key", "");
        if (key.empty()) continue;
        std::string type = varData.value("type", "");
        TypeInfo typeInfo = GetValueType(type);
        // LoadAnyFromJson は既に正しい型で保持された MyAny を返すため、
        // それをさらに MyAny(value, typeInfo) で包んではいけない。
        // その2引数コンストラクタは T を MyAny 自身と推論して Holder<MyAny> を
        // 構築した上で typeInfo だけ（例えば Float に）上書きしてしまうため、
        // 実データは MyAny 一個分のバイト列のまま「Float」を騙る不整合な Holder になり、
        // AnyCast<float>() 等が Holder<MyAny>* を Holder<float>* として誤って
        // reinterpret し、値が破損して見える（未定義動作）。
        sceneVariables_[key] = LoadAnyFromJson(varData.value("value", JSON()), typeInfo);
    }
    return true;
}

bool Scene::RemoveSceneVariable(const std::string &key) {
    LogScope scope;
    auto it = sceneVariables_.find(key);
    if (it == sceneVariables_.end()) return false;
    sceneVariables_.erase(it);
    return true;
}

MyAny *Scene::GetSceneVariable(const std::string &key) {
    LogScope scope;
    auto it = sceneVariables_.find(key);
    if (it != sceneVariables_.end()) {
        return &(it->second);
    }
    return nullptr;
}

const TypeInfo &Scene::GetSceneVariableTypeInfo(const std::string &key) {
    LogScope scope;
    auto *var = GetSceneVariable(key);
    if (var) {
        return var->GetTypeInfo();
    }
    throw std::runtime_error("Scene variable not found");
}

const TypeInfo &Scene::GetGlobalSceneVariableTypeInfo(const std::string &key) {
    LogScope scope;
    auto *var = GetGlobalSceneVariableInternal(key);
    if (var) {
        return var->GetTypeInfo();
    }
    throw std::runtime_error("Scene variable not found");
}

EmptyObject *Scene::CreateEmptyObject(const std::string &name, const UUID128 &objectID, size_t index) {
    LogScope scope;
    EmptyObject *newObjPtr = objectPool_.Emplace(Passkey<Scene>{}, sceneContext_.get(), name);
    // objectIDが未指定（無効なUUID）の場合、EmptyObjectのコンストラクタで自動生成された
    // 有効なUUIDをそのまま使う。ここで無条件に上書きすると、IDを指定しない全ての呼び出し
    // （スクリプトのCreateObject等）が同じ無効UUIDを共有することになり、UUIDベースの検索・削除
    // （ヒエラルキーの削除コマンド等）が正しく機能しなくなる
    if (objectID.IsValid()) {
        newObjPtr->SetObjectID(objectID);
    }
    if (index >= objects_.size()) {
        objects_.push_back(newObjPtr);
    } else {
        objects_.insert(objects_.begin() + index, newObjPtr);
    }
    objectsByUUID_[newObjPtr->GetObjectID()] = newObjPtr;
    objectsExistingSet_.insert(newObjPtr);
    objectsByName_[name].insert(newObjPtr);
    return newObjPtr;
}

EmptyObject *Scene::CloneObject(EmptyObject *source, const std::string &name) {
    LogScope scope;
    if (!source || !objectsExistingSet_.contains(source)) return nullptr;

    EmptyObject *clonedPtr = CreateEmptyObject(name.empty() ? source->GetName() : name);
    if (!clonedPtr) return nullptr;
    clonedPtr->CopyStateFrom(Passkey<Scene>{}, *source);
    return clonedPtr;
}

bool Scene::DeleteObject(EmptyObject *obj) {
    LogScope scope;
    if (!obj) return false;
    auto it = std::find(objects_.begin(), objects_.end(), obj);
    if (it == objects_.end()) return false;

    // 子オブジェクトも道連れに削除する（削除前に対象を収集してから再帰的に削除する。
    // 削除中に objects_ が変化しイテレータが無効化されるため、先に収集する必要がある）
    std::vector<EmptyObject *> children;
    for (auto *candidate : objects_) {
        if (!candidate || candidate == obj) continue;
        auto *candidateTransform = candidate->GetComponent<Transform>();
        if (candidateTransform && candidateTransform->GetParentObject() == obj) {
            children.push_back(candidate);
        }
    }
    for (auto *child : children) {
        DeleteObject(child); // 再帰呼び出しでさらに孫オブジェクトも削除される
    }

    // 子オブジェクトの削除により objects_ が変化しているため、対象オブジェクトを再検索する
    it = std::find(objects_.begin(), objects_.end(), obj);
    if (it == objects_.end()) return false;
    // シーンからは即座に見えなくする（名前/UUID検索・存在確認はここから無効になる）
    RemoveObjectFromMaps(obj);
    objects_.erase(it);

    if (isProcessingObjectLifecycle_) {
        // 更新処理中（スクリプトが自分自身の所有オブジェクトを削除する場合など）は、
        // ここで実体を破棄すると実行中のコンポーネント自身を破棄してしまい危険なため、
        // 更新が完全に終わった安全なタイミング（FlushPendingDestroys）まで実破棄を遅延する
        pendingDestroyObjects_.push_back(obj);
    } else {
        objectPool_.Remove(obj);
    }
    return true;
}

void Scene::FlushPendingDestroys() {
    LogScope scope;
    if (pendingDestroyObjects_.empty()) return;
    // 破棄処理（Finalize等）の連鎖でさらにDeleteObjectが呼ばれる場合に備え、
    // その間も遅延させつつキューが尽きるまで繰り返す
    isProcessingObjectLifecycle_ = true;
    while (!pendingDestroyObjects_.empty()) {
        std::vector<EmptyObject *> batch;
        batch.swap(pendingDestroyObjects_);
        for (EmptyObject *obj : batch) {
            objectPool_.Remove(obj);
        }
    }
    isProcessingObjectLifecycle_ = false;
}

void Scene::DeleteEditorOnlyObjects() {
    LogScope scope;
    // DeleteObjectで子孫が道連れに削除されobjects_が変化するため、先に対象を収集する
    std::vector<EmptyObject *> editorOnlyObjects;
    for (auto *object : objects_) {
        if (object && object->IsEditorOnly()) editorOnlyObjects.push_back(object);
    }
    for (auto *object : editorOnlyObjects) {
        // 先に削除された親の子孫だった場合、DeleteObjectは再検索に失敗して安全にfalseを返す
        DeleteObject(object);
    }
}

bool Scene::ReleaseObject(EmptyObject *obj) {
    LogScope scope;
    if (!obj) return false;
    auto it = std::find(objects_.begin(), objects_.end(), obj);
    if (it == objects_.end()) return false;
    RemoveObjectFromMaps(obj);
    objects_.erase(it);
    // 所有権の放棄（シーンからは見えなくなるが、プール内のインスタンス自体は破棄しない）
    return true;
}

bool Scene::MoveObject(EmptyObject *obj, size_t newIndex) {
    LogScope scope;
    if (!obj) return false;
    auto it = std::find(objects_.begin(), objects_.end(), obj);
    if (it == objects_.end()) return false;
    if (newIndex >= objects_.size()) newIndex = objects_.size() - 1;

    objects_.erase(it);
    objects_.insert(objects_.begin() + newIndex, obj);
    return true;
}

std::vector<EmptyObject *> Scene::GetSceneObjects(const std::string &objectName) const {
    LogScope scope;
    std::vector<EmptyObject *> result;
    auto it = objectsByName_.find(objectName);
    if (it == objectsByName_.end()) return result;
    // 追加順（objects_の並び）で返す
    for (auto *obj : objects_) {
        if (obj && it->second.contains(obj)) {
            result.push_back(obj);
        }
    }
    return result;
}

EmptyObject *Scene::GetSceneObject(const std::string &objectName) const {
    LogScope scope;
    auto it = objectsByName_.find(objectName);
    if (it == objectsByName_.end() || it->second.empty()) return nullptr;
    for (auto *obj : objects_) {
        if (obj && it->second.contains(obj)) {
            return obj;
        }
    }
    return nullptr;
}

EmptyObject *Scene::GetSceneObject(EmptyObject *obj) const {
    LogScope scope;
    if (!obj) return nullptr;
    return objectsExistingSet_.contains(obj) ? obj : nullptr;
}

EmptyObject *Scene::GetSceneObject(const UUID128 &uuid) const {
    LogScope scope;
    auto it = objectsByUUID_.find(uuid);
    return it != objectsByUUID_.end() ? it->second : nullptr;
}

void Scene::RemoveObjectFromMaps(EmptyObject *obj) {
    LogScope scope;
    if (!obj) return;
    objectsByUUID_.erase(obj->GetObjectID());
    objectsExistingSet_.erase(obj);
    auto nameIt = objectsByName_.find(obj->GetName());
    if (nameIt != objectsByName_.end()) {
        nameIt->second.erase(obj);
        if (nameIt->second.empty()) objectsByName_.erase(nameIt);
    }
}

void Scene::ClearSceneObjects() {
    LogScope scope;
    // FinalizeInterface を呼んでからオブジェクトを破棄する
    for (auto *obj : objects_) {
        if (obj) {
            obj->FinalizeInterface(Passkey<Scene>());
        }
    }
    objectPool_.Clear();
    // objectPool_.Clear() で一括破棄済みのため、破棄待ちキューに残ったポインタは
    // すべてダングリングになる。FlushPendingDestroysで二重に触れないようここで捨てる
    pendingDestroyObjects_.clear();
    objects_.clear();
    objectsByUUID_.clear();
    objectsExistingSet_.clear();
    objectsByName_.clear();
}

ISceneComponent *Scene::GetComponent(const ISceneComponent *component) const {
    LogScope scope;
    if (component == nullptr) return nullptr;
    auto it = componentsIndexByPointer_.find(component);
    if (it == componentsIndexByPointer_.end()) return nullptr;
    size_t index = it->second;
    if (index >= components_.size()) return nullptr;
    return components_[index].first.get();
}

size_t Scene::HasComponent(const ISceneComponent *component) const {
    LogScope scope;
    if (component == nullptr) return 0;
    auto it = componentsIndexByPointer_.find(component);
    if (it == componentsIndexByPointer_.end()) return 0;
    return 1;
}

ISceneComponent *Scene::AddComponent(std::unique_ptr<ISceneComponent> comp) {
    LogScope scope;
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
            components_[freeIndex].second = nextAddedComponentID_++;
            componentsIndexByType_[typeIndex].push_back(freeIndex);
            componentsIndexByPointer_[components_[freeIndex].first.get()] = freeIndex;
            components_[freeIndex].first->InitializeInterface(Passkey<Scene>(), sceneContext_.get());
            return components_[freeIndex].first.get();
        }
    }
    components_.push_back({ std::move(comp), nextAddedComponentID_++ });
    componentsIndexByType_[typeIndex].push_back(components_.size() - 1);
    componentsIndexByPointer_[components_.back().first.get()] = components_.size() - 1;
    components_.back().first->InitializeInterface(Passkey<Scene>(), sceneContext_.get());
    return components_.back().first.get();
}

bool Scene::RemoveComponent(const ISceneComponent *component) {
    LogScope scope;
    if (component == nullptr) return false;
    auto it = componentsIndexByPointer_.find(component);
    if (it == componentsIndexByPointer_.end()) return false;
    size_t index = it->second;
    if (index >= components_.size()) return false;
    components_[index].first->FinalizeInterface(Passkey<Scene>());
    size_t typeIndex = components_[index].first->GetComponentTypeID();
    auto &indices = componentsIndexByType_[typeIndex];
    auto indexIt = std::find(indices.begin(), indices.end(), index);
    if (indexIt != indices.end()) indices.erase(indexIt);
    componentsIndexByPointer_.erase(components_[index].first.get());
    componentsFreeIndices_.push_back(index);
    components_[index].first.reset();
    return true;
}

void Scene::ClearSceneComponents() {
    LogScope scope;
    // FinalizeInterface を呼んでからコンポーネントを破棄する
    for (auto &compPair : components_) {
        if (compPair.first) {
            compPair.first->FinalizeInterface(Passkey<Scene>());
        }
    }
    for (auto &compPair : components_) {
        compPair.first.reset();
    }
    components_.clear();
    componentsIndexByType_.clear();
    componentsIndexByPointer_.clear();
    componentsFreeIndices_.clear();
    nextAddedComponentID_ = 0;
}

bool Scene::ChangeToNextScene() {
    LogScope scope;
    if (sceneManager_ && !nextSceneName_.empty()) {
        return sceneManager_->ChangeScene(nextSceneName_);
    }
    return false;
}

void Scene::UpdateSceneObjects() {
    LogScope scope;
    if (objects_.empty()) return;

    // Update中に他のオブジェクトが生成/削除されても objects_ 自体の
    // イテレータが無効化されないよう、事前にポインタのスナップショットを取ってから回す
    // （ParticleSystem等、Update中に子オブジェクトを生成・削除するコンポーネントのため）
    std::vector<EmptyObject *> snapshot;
    snapshot.reserve(objects_.size());
    for (auto *obj : objects_) {
        if (obj) snapshot.push_back(obj);
    }

    for (EmptyObject *obj : snapshot) {
        // このフレーム中に別のオブジェクトのUpdateから削除されていたらスキップする
        if (!GetSceneObject(obj)) continue;
        if (obj->IsActive()) {
            obj->UpdateInterface(Passkey<Scene>());
        }
    }
}

void Scene::UpdateComponents() {
    LogScope scope;
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
        // Update中に別のコンポーネントから削除されている可能性がある。
        const auto it = componentsIndexByPointer_.find(info.component);
        if (it == componentsIndexByPointer_.end() || it->second >= components_.size()) continue;
        auto &[ownedComponent, addedID] = components_[it->second];
        if (!ownedComponent || addedID != info.addedID) continue;
        ISceneComponent *component = ownedComponent.get();
        if (component && component->IsActive()) {
            component->UpdateInterface(Passkey<Scene>());
        }
    }
}

void Scene::RegenerateUpdateComponentsList() {}

MyAny *Scene::AddGlobalSceneVariableInternal(const std::string &key, const MyAny &value, const TypeInfo &typeInfo) {
    LogScope scope;
    if (!sceneManager_) return nullptr;
    return sceneManager_->AddGlobalSceneVariable(key, value, typeInfo);
}

bool Scene::RemoveGlobalSceneVariableInternal(const std::string &key) {
    LogScope scope;
    if (!sceneManager_) return false;
    return sceneManager_->RemoveGlobalSceneVariable(key);
}

MyAny *Scene::GetGlobalSceneVariableInternal(const std::string &key) {
    LogScope scope;
    if (!sceneManager_) return nullptr;
    return sceneManager_->GetGlobalSceneVariable(key);
}

const std::unordered_map<std::string, MyAny> &Scene::GetGlobalSceneVariablesInternal() const {
    LogScope scope;
    if (!sceneManager_) {
        static const std::unordered_map<std::string, MyAny> emptyMap;
        return emptyMap;
    }
    return sceneManager_->GetGlobalSceneVariables();
}

} // namespace KashipanEngine
