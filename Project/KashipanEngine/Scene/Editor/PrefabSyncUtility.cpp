#include "PrefabSyncUtility.h"
#ifdef USE_IMGUI
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "ComponentSerialize/ComponentRegistry.h"
#include "Debug/Logger.h"
#include "Objects/Components/PrefabInstanceComponent.h"
#include "Objects/Components/Transform.h"
#include "Objects/EmptyObject.h"
#include "Scene/Editor/PrefabAssetManager.h"
#include "Scene/Editor/PrefabSceneSync.h"
#include "Scene/Editor/PrefabUtility.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {
namespace PrefabSyncUtility {

namespace {

/// @brief (更新優先度, 追加順ID) で並べたコンポーネント一覧を返す
///        （SceneObjectInspector::GetOrderedComponentsと同じ並び順規則）
std::vector<IObjectComponent *> GetOrderedComponents(EmptyObject *obj) {
    std::vector<std::pair<IObjectComponent *, size_t>> entries;
    for (const auto &entry : obj->GetAllComponents()) {
        if (!entry.first) continue;
        entries.emplace_back(entry.first, entry.second);
    }
    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
        if (a.first->GetUpdatePriority() != b.first->GetUpdatePriority()) {
            return a.first->GetUpdatePriority() < b.first->GetUpdatePriority();
        }
        return a.second < b.second;
    });
    std::vector<IObjectComponent *> result;
    result.reserve(entries.size());
    for (const auto &entry : entries) result.push_back(entry.first);
    return result;
}

/// @brief Prefabの"objects"配列を走査するコールバックヘルパー（entry["object"]を渡す）
template <typename Func>
void ForEachPrefabObjectJson(const JSON &prefabJson, Func &&func) {
    if (!prefabJson.contains("objects") || !prefabJson["objects"].is_array()) return;
    for (const auto &entry : prefabJson["objects"]) {
        if (!entry.contains("object")) continue;
        func(entry["object"]);
    }
}

/// @brief Prefabの"objects"配列を"prefabNodeID"で索引化する
std::unordered_map<std::string, const JSON *> IndexPrefabObjectsByNodeID(const JSON &prefabJson) {
    std::unordered_map<std::string, const JSON *> result;
    ForEachPrefabObjectJson(prefabJson, [&](const JSON &objJson) {
        const std::string id = objJson.value("prefabNodeID", std::string{});
        if (!id.empty()) result[id] = &objJson;
    });
    return result;
}

/// @brief オブジェクトJSON内の"components"配列を取得する（無ければ空配列）
const JSON &GetComponentsArray(const JSON &objectJson) {
    static const JSON kEmpty = JSON::array();
    if (!objectJson.contains("components") || !objectJson["components"].is_array()) return kEmpty;
    return objectJson["components"];
}

/// @brief components配列から「同じ型のordinal番目」の{type,data}エントリを探す
const JSON *FindComponentJsonByTypeOrdinal(const JSON &componentsArray, const std::string &typeName, size_t ordinal) {
    size_t count = 0;
    for (const auto &c : componentsArray) {
        if (c.value("type", "") != typeName) continue;
        if (count == ordinal) return &c;
        ++count;
    }
    return nullptr;
}

/// @brief オブジェクトJSON内のTransformの親参照（customData["parent"]）を消去する
///        （PrefabUtility.cppのEraseTransformParentと同じ処理。役割分担のため独立して持つ）
void EraseTransformParent(JSON &objectJson) {
    if (!objectJson.contains("components")) return;
    for (auto &compJson : objectJson["components"]) {
        if (compJson.value("type", "") != "Transform" || !compJson.contains("data")) continue;
        auto &data = compJson["data"];
        if (!data.contains("customData")) continue;
        data["customData"].erase("parent");
    }
}

/// @brief オブジェクトJSON内のPrefabInstanceComponentを消去する
///        （PrefabUtility.cppのEraseRootPrefabInstanceComponentと同じ処理）
void EraseRootPrefabInstanceComponent(JSON &objectJson) {
    if (!objectJson.contains("components")) return;
    auto &components = objectJson["components"];
    for (auto it = components.begin(); it != components.end();) {
        if (it->value("type", "") == "PrefabInstanceComponent") {
            it = components.erase(it);
        } else {
            ++it;
        }
    }
}

/// @brief 指定オブジェクトの現在のコンポーネント一覧を対応するPrefabノードと比較し、Overrideを収集する
void CollectOverridesForObject(EmptyObject *obj, const JSON &prefabNodeJson, std::vector<ComponentOverride> &out) {
    const JSON &prefabComponents = GetComponentsArray(prefabNodeJson);
    std::vector<IObjectComponent *> liveComponents = GetOrderedComponents(obj);

    std::unordered_map<std::string, size_t> liveOrdinalCount;
    for (IObjectComponent *comp : liveComponents) {
        const std::string type = comp->GetComponentType();
        const size_t ordinal = liveOrdinalCount[type]++;

        const JSON *matched = FindComponentJsonByTypeOrdinal(prefabComponents, type, ordinal);
        if (!matched) {
            out.push_back({ obj, comp, type, ordinal, false, true });
            continue;
        }
        if (obj->SaveComponentToJson(comp) != *matched) {
            out.push_back({ obj, comp, type, ordinal, true, true });
        }
    }

    // Prefab側にのみ存在する（ライブ側で削除された）コンポーネントを検出する
    std::unordered_map<std::string, size_t> prefabTypeCount;
    for (const auto &pc : prefabComponents) prefabTypeCount[pc.value("type", "")]++;
    for (const auto &[type, count] : prefabTypeCount) {
        const size_t liveCount = liveOrdinalCount.contains(type) ? liveOrdinalCount[type] : 0;
        for (size_t ordinal = liveCount; ordinal < count; ++ordinal) {
            out.push_back({ obj, nullptr, type, ordinal, true, false });
        }
    }
}

void CollectSubtreeObjectsRecursive(SceneEditorContext *context, EmptyObject *obj, std::vector<EmptyObject *> &out) {
    if (!obj) return;
    out.push_back(obj);
    for (auto *candidate : context->GetSceneObjects()) {
        if (!candidate || candidate == obj) continue;
        auto *candidateTransform = candidate->GetComponent<Transform>();
        if (candidateTransform && candidateTransform->GetParentObject() == obj) {
            CollectSubtreeObjectsRecursive(context, candidate, out);
        }
    }
}

/// @brief instanceRootのPrefabInstanceComponentを取得する（無効な場合はnullptr）
PrefabInstanceComponent *GetValidPrefabInstanceComponent(EmptyObject *obj) {
    if (!obj) return nullptr;
    auto *comp = obj->GetComponent<PrefabInstanceComponent>();
    return (comp && comp->GetPrefabID().IsValid()) ? comp : nullptr;
}

} // namespace

std::vector<EmptyObject *> CollectSubtreeObjects(SceneEditorContext *context, EmptyObject *root) {
    std::vector<EmptyObject *> result;
    if (!context || !root) return result;
    CollectSubtreeObjectsRecursive(context, root, result);
    return result;
}

EmptyObject *FindEnclosingPrefabInstanceRoot(EmptyObject *obj) {
    EmptyObject *current = obj;
    while (current) {
        if (GetValidPrefabInstanceComponent(current)) return current;
        auto *transform = current->GetComponent<Transform>();
        current = transform ? transform->GetParentObject() : nullptr;
    }
    return nullptr;
}

bool HasComponentOverride(SceneEditorContext *context, EmptyObject *obj) {
    if (!context || !obj || !obj->GetPrefabNodeID().IsValid()) return false;
    EmptyObject *root = FindEnclosingPrefabInstanceRoot(obj);
    auto *prefabComp = GetValidPrefabInstanceComponent(root);
    if (!prefabComp) return false;

    const JSON prefabJson = PrefabAssetManager::LoadPrefabJson(prefabComp->GetPrefabID());
    const JSON *nodeJson = nullptr;
    const std::string nodeIDStr = obj->GetPrefabNodeID().ToString();
    ForEachPrefabObjectJson(prefabJson, [&](const JSON &objJson) {
        if (!nodeJson && objJson.value("prefabNodeID", std::string{}) == nodeIDStr) nodeJson = &objJson;
    });
    if (!nodeJson) return false;

    std::vector<ComponentOverride> overrides;
    CollectOverridesForObject(obj, *nodeJson, overrides);
    return !overrides.empty();
}

bool HasComponentOverrideCached(SceneEditorContext *context, EmptyObject *obj, OverrideCheckCache &cache) {
    if (!context || !obj || !obj->GetPrefabNodeID().IsValid()) return false;
    EmptyObject *root = FindEnclosingPrefabInstanceRoot(obj);
    auto *prefabComp = GetValidPrefabInstanceComponent(root);
    if (!prefabComp) return false;

    const UUID128 prefabID = prefabComp->GetPrefabID();
    auto cacheIt = cache.nodeIndexByPrefab.find(prefabID);
    if (cacheIt == cache.nodeIndexByPrefab.end()) {
        // ここで初めてPrefab JSONへアクセスする（コピーを避けるため参照版を使う）。
        // 索引はPrefab単位で1回だけ構築し、以後同じPrefabに属する行はこのキャッシュを再利用する
        const JSON &prefabJson = PrefabAssetManager::LoadPrefabJsonRef(prefabID);
        cacheIt = cache.nodeIndexByPrefab.emplace(prefabID, IndexPrefabObjectsByNodeID(prefabJson)).first;
    }
    const auto &nodeIndex = cacheIt->second;
    const auto nodeIt = nodeIndex.find(obj->GetPrefabNodeID().ToString());
    if (nodeIt == nodeIndex.end()) return false;

    std::vector<ComponentOverride> overrides;
    CollectOverridesForObject(obj, *nodeIt->second, overrides);
    return !overrides.empty();
}

bool HasAnyOverride(SceneEditorContext *context, EmptyObject *instanceRoot) {
    return !Diff(context, instanceRoot).IsEmpty();
}

SyncReport Diff(SceneEditorContext *context, EmptyObject *instanceRoot) {
    SyncReport report;
    report.instanceRoot = instanceRoot;
    auto *prefabComp = GetValidPrefabInstanceComponent(instanceRoot);
    if (!context || !prefabComp) return report;
    report.prefabID = prefabComp->GetPrefabID();

    const JSON prefabJson = PrefabAssetManager::LoadPrefabJson(report.prefabID);
    const auto prefabByID = IndexPrefabObjectsByNodeID(prefabJson);

    std::vector<EmptyObject *> subtree = CollectSubtreeObjects(context, instanceRoot);
    std::unordered_map<std::string, bool> matchedPrefabNodeIDs;
    for (const auto &[id, json] : prefabByID) matchedPrefabNodeIDs[id] = false;

    for (EmptyObject *obj : subtree) {
        const UUID128 &nodeID = obj->GetPrefabNodeID();
        const std::string nodeIDStr = nodeID.ToString();
        auto it = nodeID.IsValid() ? prefabByID.find(nodeIDStr) : prefabByID.end();
        if (it == prefabByID.end()) {
            report.instanceOnlyObjects.push_back(obj);
            continue;
        }
        matchedPrefabNodeIDs[nodeIDStr] = true;
        CollectOverridesForObject(obj, *it->second, report.overrides);
    }
    for (const auto &[id, matched] : matchedPrefabNodeIDs) {
        if (!matched) report.prefabOnlyNodeIDs.push_back(UUID128(id));
    }
    return report;
}

bool ApplyAll(SceneEditorContext *context, EmptyObject *instanceRoot) {
    auto *prefabComp = GetValidPrefabInstanceComponent(instanceRoot);
    if (!context || !prefabComp) return false;
    const UUID128 prefabID = prefabComp->GetPrefabID();

    std::vector<EmptyObject *> subtree = CollectSubtreeObjects(context, instanceRoot);
    if (subtree.empty()) return false;

    std::unordered_map<EmptyObject *, int> indexOf;
    for (size_t i = 0; i < subtree.size(); ++i) indexOf[subtree[i]] = static_cast<int>(i);

    JSON newPrefabJson = JSON::object();
    newPrefabJson["prefab"] = true;
    newPrefabJson["prefabID"] = prefabID.ToString();
    newPrefabJson["name"] = instanceRoot->GetName();
    newPrefabJson["objects"] = JSON::array();

    for (EmptyObject *obj : subtree) {
        // ローカルで追加され、まだprefabNodeIDを持たないオブジェクトには、この場で新規発行して書き戻す
        if (!obj->GetPrefabNodeID().IsValid()) {
            obj->SetPrefabNodeID(UUID128(true));
        }
        JSON objectJson = context->SaveObjectToJson(obj);

        auto *transform = obj->GetComponent<Transform>();
        EmptyObject *parent = transform ? transform->GetParentObject() : nullptr;
        int parentIndex = -1;
        if (parent) {
            auto it = indexOf.find(parent);
            if (it != indexOf.end()) parentIndex = it->second;
        }
        if (parentIndex < 0) {
            EraseTransformParent(objectJson);
            EraseRootPrefabInstanceComponent(objectJson);
        }

        JSON entry;
        entry["parentIndex"] = parentIndex;
        entry["object"] = std::move(objectJson);
        newPrefabJson["objects"].push_back(std::move(entry));
    }

    // インスタンス側で削除されたオブジェクトは、subtreeだけから構築するため自然にPrefab側からも除外される
    return PrefabAssetManager::SavePrefabJson(prefabID, newPrefabJson);
}

bool RevertAll(SceneEditorContext *context, SceneEditorCommands *commands, EmptyObject *instanceRoot) {
    auto *prefabComp = GetValidPrefabInstanceComponent(instanceRoot);
    if (!context || !prefabComp) return false;
    const UUID128 prefabID = prefabComp->GetPrefabID();

    const JSON prefabJson = PrefabAssetManager::LoadPrefabJson(prefabID);
    auto prefabNodes = PrefabUtility::LoadPrefabNodes(prefabJson);
    if (prefabNodes.empty()) return false;
    auto preparedNodes = PrefabUtility::PrepareNodesForInstantiation(prefabNodes, /*preserveRootParent=*/false);

    auto *transform = instanceRoot->GetComponent<Transform>();
    EmptyObject *attachParent = transform ? transform->GetParentObject() : nullptr;
    const size_t insertIndex = context->GetObjectIndex(instanceRoot);
    const std::string name = instanceRoot->GetName();

    auto composite = std::make_unique<CompositeCommand>(Translation("editor.command.revertprefabinstance") + name);
    composite->AddCommand(std::make_unique<DeleteObjectCommand>(instanceRoot));
    composite->AddCommand(std::make_unique<PasteObjectCommand>(
        std::move(preparedNodes), attachParent, insertIndex, name, "Revert Prefab Instance"));

    if (commands) return commands->Execute(std::move(composite));
    return composite->Execute(context);
}

bool ApplyComponentOverride(SceneEditorContext *context, const ComponentOverride &target) {
    if (!context || !target.object) return false;
    auto *prefabComp = GetValidPrefabInstanceComponent(FindEnclosingPrefabInstanceRoot(target.object));
    if (!prefabComp) return false;
    const UUID128 prefabID = prefabComp->GetPrefabID();

    JSON prefabJson = PrefabAssetManager::LoadPrefabJson(prefabID);
    if (!prefabJson.contains("objects") || !prefabJson["objects"].is_array()) return false;
    const std::string nodeIDStr = target.object->GetPrefabNodeID().ToString();

    for (auto &entry : prefabJson["objects"]) {
        if (!entry.contains("object")) continue;
        auto &objectJson = entry["object"];
        if (objectJson.value("prefabNodeID", std::string{}) != nodeIDStr) continue;
        if (!objectJson.contains("components") || !objectJson["components"].is_array()) {
            objectJson["components"] = JSON::array();
        }
        auto &components = objectJson["components"];

        size_t seen = 0;
        int foundIndex = -1;
        for (size_t i = 0; i < components.size(); ++i) {
            if (components[i].value("type", "") != target.componentType) continue;
            if (seen == target.ordinal) { foundIndex = static_cast<int>(i); break; }
            ++seen;
        }

        if (target.component) {
            const JSON newEntry = target.object->SaveComponentToJson(target.component);
            if (foundIndex >= 0) components[foundIndex] = newEntry;
            else components.push_back(newEntry);
        } else if (foundIndex >= 0) {
            components.erase(components.begin() + foundIndex);
        }
        break;
    }

    return PrefabAssetManager::SavePrefabJson(prefabID, prefabJson);
}

bool RevertComponentOverride(SceneEditorContext *context, SceneEditorCommands *commands, const ComponentOverride &target) {
    if (!context || !target.object) return false;
    auto *prefabComp = GetValidPrefabInstanceComponent(FindEnclosingPrefabInstanceRoot(target.object));
    if (!prefabComp) return false;

    const JSON prefabJson = PrefabAssetManager::LoadPrefabJson(prefabComp->GetPrefabID());
    const std::string nodeIDStr = target.object->GetPrefabNodeID().ToString();
    const JSON *nodeJson = nullptr;
    ForEachPrefabObjectJson(prefabJson, [&](const JSON &objJson) {
        if (!nodeJson && objJson.value("prefabNodeID", std::string{}) == nodeIDStr) nodeJson = &objJson;
    });
    const JSON *matched = nodeJson ? FindComponentJsonByTypeOrdinal(GetComponentsArray(*nodeJson), target.componentType, target.ordinal) : nullptr;

    if (!matched) {
        // Prefab側に対応が無い＝ローカル追加コンポーネント。元に戻す＝削除する
        if (!target.component) return false;
        if (commands) return commands->Execute(std::make_unique<RemoveComponentCommand>(target.object, target.component));
        return target.object->RemoveComponent(target.component);
    }

    if (!target.component) {
        // ローカルで削除済み。元に戻す＝Prefabの内容で追加し直す（追加と同時に値も復元する）
        if (commands) {
            return commands->Execute(std::make_unique<AddComponentCommand>(target.object, target.componentType, *matched));
        }
        IObjectComponent *newComp = target.object->AddComponent(CreateObjectComponentByType(target.componentType));
        return newComp && target.object->LoadComponentFromJson(newComp, *matched);
    }

    // 値の差し戻し
    if (commands) {
        const JSON before = target.object->SaveComponentToJson(target.component);
        return commands->Execute(std::make_unique<ComponentEditCommand>(target.object, target.component, before, *matched));
    }
    return target.object->LoadComponentFromJson(target.component, *matched);
}

namespace {

std::string GetObjectParentID(const JSON &objectJson) {
    const JSON &components = GetComponentsArray(objectJson);
    for (const auto &component : components) {
        if (component.value("type", std::string{}) != "Transform" || !component.contains("data")) continue;
        const JSON &data = component["data"];
        if (!data.contains("customData") || !data["customData"].is_object()) return {};
        return data["customData"].value("parent", std::string{});
    }
    return {};
}

using JsonObjectByID = std::unordered_map<std::string, const JSON *>;

JsonObjectByID IndexSceneObjectsByID(const JSON &sceneJson) {
    JsonObjectByID result;
    if (!sceneJson.contains("sceneObjects") || !sceneJson["sceneObjects"].is_array()) return result;
    for (const auto &object : sceneJson["sceneObjects"]) {
        const std::string objectID = object.value("objectID", std::string{});
        if (!objectID.empty()) result[objectID] = &object;
    }
    return result;
}

using ComponentJsonByKey = std::unordered_map<std::string, const JSON *>;

ComponentJsonByKey IndexComponentJsonByKey(const JSON &components) {
    ComponentJsonByKey result;
    std::unordered_map<std::string, size_t> ordinals;
    if (!components.is_array()) return result;
    for (const auto &component : components) {
        const std::string type = component.value("type", std::string{});
        const size_t ordinal = ordinals[type]++;
        result[type + "\x1f" + std::to_string(ordinal)] = &component;
    }
    return result;
}

using LiveComponentByKey = std::unordered_map<std::string, IObjectComponent *>;

LiveComponentByKey IndexLiveComponentsByKey(EmptyObject *object) {
    LiveComponentByKey result;
    std::unordered_map<std::string, size_t> ordinals;
    for (IObjectComponent *component : GetOrderedComponents(object)) {
        const std::string type = component->GetComponentType();
        const size_t ordinal = ordinals[type]++;
        result[type + "\x1f" + std::to_string(ordinal)] = component;
    }
    return result;
}

bool HasObjectPropertyDifference(const JSON &before, const JSON &after) {
    static constexpr const char *kKeys[] = { "name", "tag", "isActive", "editorOnly", "hiddenFromEditorTarget" };
    for (const char *key : kKeys) {
        if (before.value(key, JSON()) != after.value(key, JSON())) return true;
    }
    return false;
}

void AddExistingObjectDiffCommands(
    CompositeCommand &composite,
    EmptyObject *liveObject,
    const JSON &beforeObject,
    const JSON &afterObject) {
    if (!liveObject) return;
    if (HasObjectPropertyDifference(beforeObject, afterObject)) {
        composite.AddCommand(std::make_unique<ObjectStateCommand>(liveObject, beforeObject, afterObject));
    }

    const auto beforeComponents = IndexComponentJsonByKey(GetComponentsArray(beforeObject));
    const auto afterComponents = IndexComponentJsonByKey(GetComponentsArray(afterObject));
    const auto liveComponents = IndexLiveComponentsByKey(liveObject);
    std::unordered_set<std::string> allKeys;
    for (const auto &[key, value] : beforeComponents) allKeys.insert(key);
    for (const auto &[key, value] : afterComponents) allKeys.insert(key);

    for (const auto &key : allKeys) {
        const auto beforeIt = beforeComponents.find(key);
        const auto afterIt = afterComponents.find(key);
        const auto liveIt = liveComponents.find(key);
        const JSON *before = beforeIt != beforeComponents.end() ? beforeIt->second : nullptr;
        const JSON *after = afterIt != afterComponents.end() ? afterIt->second : nullptr;
        IObjectComponent *live = liveIt != liveComponents.end() ? liveIt->second : nullptr;
        const size_t separator = key.rfind('\x1f');
        const std::string type = separator == std::string::npos ? key : key.substr(0, separator);

        if (before && after) {
            if (*before != *after && live) {
                composite.AddCommand(std::make_unique<ComponentEditCommand>(
                    liveObject, live, *before, *after));
            }
        } else if (!before && after) {
            composite.AddCommand(std::make_unique<AddComponentCommand>(liveObject, type, *after));
        } else if (before && !after && live) {
            composite.AddCommand(std::make_unique<RemoveComponentCommand>(liveObject, live));
        }
    }
}

void AddNewObjectCommands(
    CompositeCommand &composite,
    const JSON &beforeScene,
    const JSON &afterScene) {
    const auto beforeByID = IndexSceneObjectsByID(beforeScene);
    if (!afterScene.contains("sceneObjects") || !afterScene["sceneObjects"].is_array()) return;

    std::vector<PasteObjectCommand::Node> nodes;
    std::unordered_map<std::string, int> nodeIndexByObjectID;
    for (const auto &object : afterScene["sceneObjects"]) {
        const std::string objectID = object.value("objectID", std::string{});
        if (objectID.empty() || beforeByID.contains(objectID)) continue;
        int parentIndex = -1;
        const std::string parentID = GetObjectParentID(object);
        auto parentIt = nodeIndexByObjectID.find(parentID);
        if (parentIt != nodeIndexByObjectID.end()) parentIndex = parentIt->second;
        nodeIndexByObjectID[objectID] = static_cast<int>(nodes.size());
        nodes.push_back({ object, parentIndex });
    }
    if (nodes.empty()) return;
    composite.AddCommand(std::make_unique<PasteObjectCommand>(
        std::move(nodes), nullptr, MAXSIZE_T, "Prefab Nodes", "Sync Prefab Nodes",
        /*preserveOriginalRootParent=*/true));
}

void AddRemovedObjectCommands(
    CompositeCommand &composite,
    SceneEditorContext *context,
    const JSON &beforeScene,
    const JSON &afterScene) {
    const auto afterByID = IndexSceneObjectsByID(afterScene);
    if (!beforeScene.contains("sceneObjects") || !beforeScene["sceneObjects"].is_array()) return;

    std::unordered_set<std::string> removedIDs;
    for (const auto &object : beforeScene["sceneObjects"]) {
        const std::string objectID = object.value("objectID", std::string{});
        if (!objectID.empty() && !afterByID.contains(objectID)) removedIDs.insert(objectID);
    }
    for (const auto &object : beforeScene["sceneObjects"]) {
        const std::string objectID = object.value("objectID", std::string{});
        if (!removedIDs.contains(objectID)) continue;
        // 親も削除対象なら親のDeleteObjectCommandがサブツリーごと削除する。
        if (removedIDs.contains(GetObjectParentID(object))) continue;
        if (EmptyObject *liveObject = context->GetSceneObject(UUID128(objectID))) {
            composite.AddCommand(std::make_unique<DeleteObjectCommand>(liveObject));
        }
    }
}

} // namespace

void SyncOtherInstances(SceneEditorContext *context, SceneEditorCommands *commands,
    const UUID128 &prefabID, const JSON &oldPrefabJson, const JSON &newPrefabJson) {
    if (!context || !commands || !prefabID.IsValid()) return;

    const JSON beforeScene = context->SaveSceneToJSON();
    JSON afterScene = beforeScene;
    const PrefabSceneSync::SceneSyncResult syncResult =
        PrefabSceneSync::SyncSceneJson(afterScene, prefabID, oldPrefabJson, newPrefabJson);
    if (!syncResult.IsChanged()) return;

    auto composite = std::make_unique<CompositeCommand>(Translation("editor.command.syncprefab"));

    // 新規ノードを先に生成する。既存ノードのTransformがその新規ノードへ親変更される場合、
    // 後続のComponentEditCommand実行時に親objectIDを解決できるようにするため。
    AddNewObjectCommands(*composite, beforeScene, afterScene);

    const auto beforeByID = IndexSceneObjectsByID(beforeScene);
    const auto afterByID = IndexSceneObjectsByID(afterScene);
    for (const auto &[objectID, beforeObject] : beforeByID) {
        auto afterIt = afterByID.find(objectID);
        if (afterIt == afterByID.end()) continue;
        EmptyObject *liveObject = context->GetSceneObject(UUID128(objectID));
        AddExistingObjectDiffCommands(*composite, liveObject, *beforeObject, *afterIt->second);
    }

    // 親変更で削除対象ノードの外へ移動する既存ノードがあるため、削除は最後に実行する。
    AddRemovedObjectCommands(*composite, context, beforeScene, afterScene);
    if (!composite->IsEmpty()) commands->Execute(std::move(composite));
}

void SyncAllScenes(SceneEditorContext *context, SceneEditorCommands *commands,
    const UUID128 &prefabID, const JSON &oldPrefabJson, const JSON &newPrefabJson) {
    SyncOtherInstances(context, commands, prefabID, oldPrefabJson, newPrefabJson);
    const PrefabSceneSync::RegisteredScenesSyncResult result =
        PrefabSceneSync::SyncRegisteredScenes(context, prefabID, oldPrefabJson, newPrefabJson);

    LogScope scope;
    Log(Translation("engine.prefab.sync.result.scanned") + std::to_string(result.scenesScanned) +
        Translation("engine.prefab.sync.result.updated") + std::to_string(result.scenesChanged) +
        Translation("engine.prefab.sync.result.matched") + std::to_string(result.instancesMatched),
        result.failedScenes.empty() ? LogSeverity::Info : LogSeverity::Warning);
    for (const auto &failedScene : result.failedScenes) {
        Log(Translation("engine.prefab.sync.scene.failed") + failedScene, LogSeverity::Warning);
    }
}

} // namespace PrefabSyncUtility
} // namespace KashipanEngine
#endif // USE_IMGUI
