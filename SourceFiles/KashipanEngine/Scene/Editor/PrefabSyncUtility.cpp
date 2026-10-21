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

std::unordered_map<std::string, std::string> IndexPrefabParentNodeIDs(const JSON &prefabJson) {
    std::unordered_map<std::string, std::string> result;
    if (!prefabJson.contains("objects") || !prefabJson["objects"].is_array()) return result;
    const JSON &entries = prefabJson["objects"];
    for (size_t i = 0; i < entries.size(); ++i) {
        if (!entries[i].contains("object")) continue;
        const std::string nodeID = entries[i]["object"].value("prefabNodeID", std::string{});
        if (nodeID.empty()) continue;
        const int parentIndex = entries[i].value("parentIndex", -1);
        if (parentIndex >= 0 && static_cast<size_t>(parentIndex) < entries.size() &&
            entries[static_cast<size_t>(parentIndex)].contains("object")) {
            result[nodeID] = entries[static_cast<size_t>(parentIndex)]["object"].value(
                "prefabNodeID", std::string{});
        } else {
            result[nodeID] = {};
        }
    }
    return result;
}

bool HasObjectPropertyOverride(EmptyObject *object, const JSON &prefabObject) {
    return object->GetName() != prefabObject.value("name", std::string{}) ||
        object->GetTagName() != prefabObject.value("tag", std::string{}) ||
        object->IsActive() != prefabObject.value("isActive", true) ||
        object->IsEditorOnly() != prefabObject.value("editorOnly", false) ||
        object->IsHiddenFromEditorTarget() != prefabObject.value("hiddenFromEditorTarget", false);
}

bool HasParentOverride(EmptyObject *object, const std::string &nodeID, const JSON &prefabJson) {
    const auto prefabByNodeID = IndexPrefabObjectsByNodeID(prefabJson);
    const auto prefabParentByNodeID = IndexPrefabParentNodeIDs(prefabJson);
    std::string liveParentNodeID;
    if (auto *transform = object->GetComponent<Transform>()) {
        EmptyObject *parent = transform->GetParentObject();
        if (parent && parent->GetPrefabNodeID().IsValid() &&
            prefabByNodeID.contains(parent->GetPrefabNodeID().ToString())) {
            liveParentNodeID = parent->GetPrefabNodeID().ToString();
        }
    }
    auto expected = prefabParentByNodeID.find(nodeID);
    const std::string expectedParentNodeID = expected != prefabParentByNodeID.end() ? expected->second : std::string{};
    return liveParentNodeID != expectedParentNodeID;
}

/// @brief Transformの親参照は構造Overrideとして扱い、コンポーネント値の比較から除外する
JSON NormalizeComponentForOverrideComparison(JSON componentJson) {
    if (componentJson.value("type", std::string{}) != "Transform" || !componentJson.contains("data")) {
        return componentJson;
    }
    JSON &data = componentJson["data"];
    if (data.contains("customData") && data["customData"].is_object()) {
        data["customData"].erase("parent");
    }
    return componentJson;
}

/// @brief sourceのTransform親参照だけをdestinationへコピーする（無ければdestinationから消去）
void CopyTransformParent(const JSON &source, JSON &destination) {
    if (destination.value("type", std::string{}) != "Transform" || !destination.contains("data")) return;
    JSON &destinationData = destination["data"];
    if (!destinationData.contains("customData") || !destinationData["customData"].is_object()) {
        destinationData["customData"] = JSON::object();
    }
    const JSON *sourceCustomData = nullptr;
    if (source.contains("data") && source["data"].contains("customData") && source["data"]["customData"].is_object()) {
        sourceCustomData = &source["data"]["customData"];
    }
    if (sourceCustomData && sourceCustomData->contains("parent")) {
        destinationData["customData"]["parent"] = (*sourceCustomData)["parent"];
    } else {
        destinationData["customData"].erase("parent");
    }
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
void CollectOverridesForObject(EmptyObject *obj, const JSON &prefabNodeJson,
    const std::unordered_map<std::string, std::string> &prefabToInstanceObjectID,
    std::vector<ComponentOverride> &out) {
    const JSON &prefabComponents = GetComponentsArray(prefabNodeJson);
    std::vector<IObjectComponent *> liveComponents = GetOrderedComponents(obj);

    std::unordered_map<std::string, size_t> liveOrdinalCount;
    for (IObjectComponent *comp : liveComponents) {
        const std::string type = comp->GetComponentType();
        if (type == "PrefabInstanceComponent") continue;
        const size_t ordinal = liveOrdinalCount[type]++;

        const JSON *matched = FindComponentJsonByTypeOrdinal(prefabComponents, type, ordinal);
        if (!matched) {
            out.push_back({ obj, comp, type, ordinal, false, true });
            continue;
        }
        JSON prefabComponent = *matched;
        PrefabUtility::RemapObjectIDReferences(prefabComponent, prefabToInstanceObjectID);
        if (NormalizeComponentForOverrideComparison(obj->SaveComponentToJson(comp)) !=
            NormalizeComponentForOverrideComparison(std::move(prefabComponent))) {
            out.push_back({ obj, comp, type, ordinal, true, true });
        }
    }

    // Prefab側にのみ存在する（ライブ側で削除された）コンポーネントを検出する
    std::unordered_map<std::string, size_t> prefabTypeCount;
    for (const auto &pc : prefabComponents) {
        const std::string type = pc.value("type", "");
        if (type != "PrefabInstanceComponent") prefabTypeCount[type]++;
    }
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

namespace {

std::unordered_map<std::string, std::string> BuildPrefabToInstanceObjectIDRemap(
    SceneEditorContext *context, EmptyObject *instanceRoot, const JSON &prefabJson) {
    std::unordered_map<std::string, std::string> result;
    const auto prefabByNodeID = IndexPrefabObjectsByNodeID(prefabJson);
    for (EmptyObject *object : CollectSubtreeObjects(context, instanceRoot)) {
        if (!object || !object->GetPrefabNodeID().IsValid()) continue;
        auto prefabIt = prefabByNodeID.find(object->GetPrefabNodeID().ToString());
        if (prefabIt == prefabByNodeID.end()) continue;
        const std::string prefabObjectID = prefabIt->second->value("objectID", std::string{});
        if (!prefabObjectID.empty()) result[prefabObjectID] = object->GetObjectID().ToString();
    }
    return result;
}

std::unordered_map<std::string, std::string> ReverseRemap(
    const std::unordered_map<std::string, std::string> &source) {
    std::unordered_map<std::string, std::string> result;
    result.reserve(source.size());
    for (const auto &[from, to] : source) result[to] = from;
    return result;
}

} // namespace

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
    const auto objectIDRemap = BuildPrefabToInstanceObjectIDRemap(context, root, prefabJson);
    const JSON *nodeJson = nullptr;
    const std::string nodeIDStr = obj->GetPrefabNodeID().ToString();
    ForEachPrefabObjectJson(prefabJson, [&](const JSON &objJson) {
        if (!nodeJson && objJson.value("prefabNodeID", std::string{}) == nodeIDStr) nodeJson = &objJson;
    });
    if (!nodeJson) return false;

    if (HasObjectPropertyOverride(obj, *nodeJson) || HasParentOverride(obj, nodeIDStr, prefabJson)) return true;

    std::vector<ComponentOverride> overrides;
    CollectOverridesForObject(obj, *nodeJson, objectIDRemap, overrides);
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

    const JSON &prefabJson = PrefabAssetManager::LoadPrefabJsonRef(prefabID);
    if (HasObjectPropertyOverride(obj, *nodeIt->second) ||
        HasParentOverride(obj, obj->GetPrefabNodeID().ToString(), prefabJson)) return true;
    const auto objectIDRemap = BuildPrefabToInstanceObjectIDRemap(
        context, root, prefabJson);
    std::vector<ComponentOverride> overrides;
    CollectOverridesForObject(obj, *nodeIt->second, objectIDRemap, overrides);
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
    const auto prefabParentByNodeID = IndexPrefabParentNodeIDs(prefabJson);
    const auto objectIDRemap = BuildPrefabToInstanceObjectIDRemap(context, instanceRoot, prefabJson);

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
        CollectOverridesForObject(obj, *it->second, objectIDRemap, report.overrides);
        if (HasObjectPropertyOverride(obj, *it->second)) report.objectPropertyOverrides.push_back(obj);

        std::string liveParentNodeID;
        if (auto *transform = obj->GetComponent<Transform>()) {
            EmptyObject *parent = transform->GetParentObject();
            if (parent && parent->GetPrefabNodeID().IsValid() && prefabByID.contains(parent->GetPrefabNodeID().ToString())) {
                liveParentNodeID = parent->GetPrefabNodeID().ToString();
            }
        }
        auto expectedParent = prefabParentByNodeID.find(nodeIDStr);
        const std::string expectedParentNodeID = expectedParent != prefabParentByNodeID.end() ? expectedParent->second : std::string{};
        if (liveParentNodeID != expectedParentNodeID) report.parentOverrides.push_back(obj);
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

    // 対応するPrefabノードには既存インスタンスのobjectIDを再利用し、インスタンス外からの
    // ComponentRef/objectID参照をRevert後も有効に保つ。Prefab側で追加されたノードだけ新規採番する。
    std::unordered_map<std::string, UUID128> existingObjectIDsByNodeID;
    for (EmptyObject *object : CollectSubtreeObjects(context, instanceRoot)) {
        if (object && object->GetPrefabNodeID().IsValid()) {
            existingObjectIDsByNodeID[object->GetPrefabNodeID().ToString()] = object->GetObjectID();
        }
    }

    // Prefab資産にはリンク元を埋め込まない設計なので、置換後のルートへ現在のリンク情報を明示的に戻す。
    JSON rootPrefabComponent = instanceRoot->SaveComponentToJson(prefabComp);
    JSON &rootObjectJson = prefabNodes.front().json;
    EraseRootPrefabInstanceComponent(rootObjectJson);
    if (!rootObjectJson.contains("components") || !rootObjectJson["components"].is_array()) {
        rootObjectJson["components"] = JSON::array();
    }
    rootObjectJson["components"].push_back(std::move(rootPrefabComponent));

    auto preparedNodes = PrefabUtility::PrepareNodesForInstantiation(
        prefabNodes, /*preserveRootParent=*/false, &existingObjectIDsByNodeID);

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
    const auto instanceToPrefabObjectID = ReverseRemap(
        BuildPrefabToInstanceObjectIDRemap(context, FindEnclosingPrefabInstanceRoot(target.object), prefabJson));
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
            JSON newEntry = target.object->SaveComponentToJson(target.component);
            PrefabUtility::RemapObjectIDReferences(newEntry, instanceToPrefabObjectID);
            // 親変更はコンポーネント値の個別Apply対象外。Prefab側の構造を維持する。
            if (foundIndex >= 0) CopyTransformParent(components[foundIndex], newEntry);
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
    EmptyObject *instanceRoot = FindEnclosingPrefabInstanceRoot(target.object);
    const auto prefabToInstanceObjectID = BuildPrefabToInstanceObjectIDRemap(context, instanceRoot, prefabJson);
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

    JSON adaptedPrefabComponent = *matched;
    PrefabUtility::RemapObjectIDReferences(adaptedPrefabComponent, prefabToInstanceObjectID);

    if (!target.component) {
        // ローカルで削除済み。元に戻す＝Prefabの内容で追加し直す（追加と同時に値も復元する）
        if (commands) {
            return commands->Execute(std::make_unique<AddComponentCommand>(
                target.object, target.componentType, adaptedPrefabComponent));
        }
        IObjectComponent *newComp = target.object->AddComponent(CreateObjectComponentByType(target.componentType));
        return newComp && target.object->LoadComponentFromJson(newComp, adaptedPrefabComponent);
    }

    // 値の差し戻し
    // 親変更は構造Overrideとして独立して扱い、個別コンポーネントRevertでは現在の親を維持する。
    CopyTransformParent(target.object->SaveComponentToJson(target.component), adaptedPrefabComponent);
    if (commands) {
        const JSON before = target.object->SaveComponentToJson(target.component);
        return commands->Execute(std::make_unique<ComponentEditCommand>(
            target.object, target.component, before, adaptedPrefabComponent));
    }
    return target.object->LoadComponentFromJson(target.component, adaptedPrefabComponent);
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
