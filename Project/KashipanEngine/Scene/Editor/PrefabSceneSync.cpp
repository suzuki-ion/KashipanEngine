#include "PrefabSceneSync.h"
#ifdef USE_IMGUI

#include <algorithm>
#include <deque>
#include <unordered_map>
#include <unordered_set>

#include "Debug/Logger.h"
#include "Scene/Editor/PrefabUtility.h"

namespace KashipanEngine {
namespace PrefabSceneSync {

namespace {

struct PrefabNode {
    const JSON *object = nullptr;
    std::string nodeID;
    std::string parentNodeID;
    size_t sourceIndex = 0;
};

struct PrefabGraph {
    std::vector<PrefabNode> nodes;
    std::unordered_map<std::string, const PrefabNode *> byID;
};

struct SceneIndex {
    std::unordered_map<std::string, size_t> objectIndexByID;
    std::unordered_map<std::string, std::vector<std::string>> childrenByParentID;
};

const JSON &GetComponents(const JSON &objectJson) {
    LogScope scope;
    static const JSON kEmpty = JSON::array();
    if (!objectJson.contains("components") || !objectJson["components"].is_array()) return kEmpty;
    return objectJson["components"];
}

JSON &GetOrCreateComponents(JSON &objectJson) {
    LogScope scope;
    if (!objectJson.contains("components") || !objectJson["components"].is_array()) {
        objectJson["components"] = JSON::array();
    }
    return objectJson["components"];
}

JSON *FindTransformComponent(JSON &objectJson) {
    LogScope scope;
    JSON &components = GetOrCreateComponents(objectJson);
    for (auto &component : components) {
        if (component.value("type", std::string{}) == "Transform") return &component;
    }
    return nullptr;
}

const JSON *FindTransformComponent(const JSON &objectJson) {
    LogScope scope;
    for (const auto &component : GetComponents(objectJson)) {
        if (component.value("type", std::string{}) == "Transform") return &component;
    }
    return nullptr;
}

std::string GetTransformParentID(const JSON &objectJson) {
    LogScope scope;
    const JSON *transform = FindTransformComponent(objectJson);
    if (!transform || !transform->contains("data")) return {};
    const JSON &data = (*transform)["data"];
    if (!data.contains("customData") || !data["customData"].is_object()) return {};
    return data["customData"].value("parent", std::string{});
}

void SetTransformParentID(JSON &objectJson, const std::string &parentID) {
    LogScope scope;
    JSON *transform = FindTransformComponent(objectJson);
    if (!transform || !transform->contains("data")) return;
    JSON &data = (*transform)["data"];
    if (!data.contains("customData") || !data["customData"].is_object()) {
        data["customData"] = JSON::object();
    }
    if (parentID.empty()) {
        data["customData"].erase("parent");
    } else {
        data["customData"]["parent"] = parentID;
    }
}

JSON NormalizeComponentForComparison(const JSON &component) {
    LogScope scope;
    JSON normalized = component;
    if (normalized.value("type", std::string{}) != "Transform" || !normalized.contains("data")) {
        return normalized;
    }
    JSON &data = normalized["data"];
    if (data.contains("customData") && data["customData"].is_object()) {
        // 親参照はインスタンスごとにobjectIDが異なるため、値Override判定から分離する。
        data["customData"].erase("parent");
    }
    return normalized;
}

JSON AdaptComponentToInstance(const JSON &component, const std::string &parentObjectID) {
    LogScope scope;
    JSON adapted = component;
    if (adapted.value("type", std::string{}) != "Transform" || !adapted.contains("data")) {
        return adapted;
    }
    JSON &data = adapted["data"];
    if (!data.contains("customData") || !data["customData"].is_object()) {
        data["customData"] = JSON::object();
    }
    if (parentObjectID.empty()) {
        data["customData"].erase("parent");
    } else {
        data["customData"]["parent"] = parentObjectID;
    }
    return adapted;
}

PrefabGraph BuildPrefabGraph(const JSON &prefabJson) {
    LogScope scope;
    PrefabGraph graph;
    if (!prefabJson.contains("objects") || !prefabJson["objects"].is_array()) return graph;

    const JSON &entries = prefabJson["objects"];
    graph.nodes.reserve(entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        const JSON &entry = entries[i];
        if (!entry.contains("object") || !entry["object"].is_object()) continue;
        const JSON &object = entry["object"];
        const std::string nodeID = object.value("prefabNodeID", std::string{});
        if (nodeID.empty()) continue;

        std::string parentNodeID;
        const int parentIndex = entry.value("parentIndex", -1);
        if (parentIndex >= 0 && static_cast<size_t>(parentIndex) < entries.size()) {
            const JSON &parentEntry = entries[static_cast<size_t>(parentIndex)];
            if (parentEntry.contains("object")) {
                parentNodeID = parentEntry["object"].value("prefabNodeID", std::string{});
            }
        }
        graph.nodes.push_back({ &object, nodeID, parentNodeID, i });
    }
    for (const auto &node : graph.nodes) graph.byID[node.nodeID] = &node;
    return graph;
}

SceneIndex BuildSceneIndex(const JSON &objects) {
    LogScope scope;
    SceneIndex index;
    if (!objects.is_array()) return index;
    for (size_t i = 0; i < objects.size(); ++i) {
        const std::string objectID = objects[i].value("objectID", std::string{});
        if (objectID.empty()) continue;
        index.objectIndexByID[objectID] = i;
        index.childrenByParentID[GetTransformParentID(objects[i])].push_back(objectID);
    }
    return index;
}

bool HasPrefabMarker(const JSON &objectJson, const UUID128 &prefabID) {
    LogScope scope;
    const std::string wanted = prefabID.ToString();
    for (const auto &component : GetComponents(objectJson)) {
        if (component.value("type", std::string{}) != "PrefabInstanceComponent" ||
            !component.contains("data")) {
            continue;
        }
        const JSON &data = component["data"];
        if (!data.contains("customData") || !data["customData"].is_object()) continue;
        if (data["customData"].value("prefabID", std::string{}) == wanted) return true;
    }
    return false;
}

std::unordered_set<std::string> CollectSubtreeObjectIDs(
    const SceneIndex &index, const std::string &rootObjectID) {
    LogScope scope;
    std::unordered_set<std::string> result;
    std::deque<std::string> pending;
    pending.push_back(rootObjectID);
    while (!pending.empty()) {
        std::string current = std::move(pending.front());
        pending.pop_front();
        if (!result.insert(current).second) continue;
        auto it = index.childrenByParentID.find(current);
        if (it == index.childrenByParentID.end()) continue;
        for (const auto &child : it->second) pending.push_back(child);
    }
    return result;
}

struct ComponentSlot {
    size_t index = 0;
    const JSON *value = nullptr;
};

using ComponentSlots = std::unordered_map<std::string, ComponentSlot>;

ComponentSlots IndexComponents(const JSON &components) {
    LogScope scope;
    ComponentSlots result;
    std::unordered_map<std::string, size_t> ordinalByType;
    if (!components.is_array()) return result;
    for (size_t i = 0; i < components.size(); ++i) {
        const std::string type = components[i].value("type", std::string{});
        const size_t ordinal = ordinalByType[type]++;
        result[type + "\x1f" + std::to_string(ordinal)] = { i, &components[i] };
    }
    return result;
}

void MergeObjectProperties(
    JSON &instanceObject,
    const JSON &oldObject,
    const JSON &newObject,
    SceneSyncResult &result) {
    LogScope scope;
    static constexpr const char *kKeys[] = { "name", "tag", "isActive", "editorOnly", "hiddenFromEditorTarget" };
    for (const char *key : kKeys) {
        const JSON oldValue = oldObject.contains(key) ? oldObject[key] : JSON();
        const JSON newValue = newObject.contains(key) ? newObject[key] : JSON();
        const JSON instanceValue = instanceObject.contains(key) ? instanceObject[key] : JSON();
        if (oldValue == newValue || instanceValue != oldValue) continue;
        if (newObject.contains(key)) {
            instanceObject[key] = newObject[key];
        } else {
            instanceObject.erase(key);
        }
        ++result.objectPropertiesChanged;
    }
}

void MergeComponents(
    JSON &instanceObject,
    const JSON &oldObject,
    const JSON &newObject,
    const std::unordered_map<std::string, std::string> &oldObjectIDRemap,
    const std::unordered_map<std::string, std::string> &newObjectIDRemap,
    SceneSyncResult &result) {
    LogScope scope;
    const JSON &oldComponents = GetComponents(oldObject);
    const JSON &newComponents = GetComponents(newObject);
    JSON &instanceComponents = GetOrCreateComponents(instanceObject);

    const ComponentSlots oldSlots = IndexComponents(oldComponents);
    const ComponentSlots newSlots = IndexComponents(newComponents);
    const ComponentSlots instanceSlots = IndexComponents(instanceComponents);

    std::unordered_set<std::string> allKeys;
    for (const auto &[key, slot] : oldSlots) allKeys.insert(key);
    for (const auto &[key, slot] : newSlots) allKeys.insert(key);

    std::unordered_map<size_t, JSON> replacements;
    std::unordered_set<size_t> removals;
    std::vector<JSON> additions;
    const std::string currentParentID = GetTransformParentID(instanceObject);

    for (const auto &key : allKeys) {
        const auto oldIt = oldSlots.find(key);
        const auto newIt = newSlots.find(key);
        const auto instanceIt = instanceSlots.find(key);
        const JSON *oldValue = oldIt != oldSlots.end() ? oldIt->second.value : nullptr;
        const JSON *newValue = newIt != newSlots.end() ? newIt->second.value : nullptr;
        const JSON *instanceValue = instanceIt != instanceSlots.end() ? instanceIt->second.value : nullptr;

        if (oldValue && newValue) {
            JSON adaptedOld = *oldValue;
            JSON adaptedNew = *newValue;
            PrefabUtility::RemapObjectIDReferences(adaptedOld, oldObjectIDRemap);
            PrefabUtility::RemapObjectIDReferences(adaptedNew, newObjectIDRemap);
            adaptedOld = AdaptComponentToInstance(adaptedOld, currentParentID);
            adaptedNew = AdaptComponentToInstance(adaptedNew, currentParentID);
            if (NormalizeComponentForComparison(adaptedOld) == NormalizeComponentForComparison(adaptedNew) ||
                !instanceValue ||
                NormalizeComponentForComparison(*instanceValue) != NormalizeComponentForComparison(adaptedOld)) {
                continue;
            }
            replacements[instanceIt->second.index] = std::move(adaptedNew);
            ++result.componentsChanged;
        } else if (!oldValue && newValue) {
            if (instanceValue) continue; // 同じ型+ordinalがローカル追加済み
            JSON adaptedNew = *newValue;
            PrefabUtility::RemapObjectIDReferences(adaptedNew, newObjectIDRemap);
            additions.push_back(AdaptComponentToInstance(adaptedNew, currentParentID));
            ++result.componentsChanged;
        } else if (oldValue && !newValue) {
            JSON adaptedOld = *oldValue;
            PrefabUtility::RemapObjectIDReferences(adaptedOld, oldObjectIDRemap);
            adaptedOld = AdaptComponentToInstance(adaptedOld, currentParentID);
            if (!instanceValue || NormalizeComponentForComparison(*instanceValue) !=
                                      NormalizeComponentForComparison(adaptedOld)) {
                continue;
            }
            removals.insert(instanceIt->second.index);
            ++result.componentsChanged;
        }
    }

    if (replacements.empty() && removals.empty() && additions.empty()) return;
    JSON merged = JSON::array();
    for (size_t i = 0; i < instanceComponents.size(); ++i) {
        if (removals.contains(i)) continue;
        auto replacement = replacements.find(i);
        merged.push_back(replacement != replacements.end() ? replacement->second : instanceComponents[i]);
    }
    for (auto &addition : additions) merged.push_back(std::move(addition));
    instanceComponents = std::move(merged);
}

std::unordered_map<std::string, std::string> BuildLiveNodeMap(
    const JSON &objects,
    const SceneIndex &index,
    const std::unordered_set<std::string> &subtreeObjectIDs) {
    LogScope scope;
    std::unordered_map<std::string, std::string> result;
    for (const auto &objectID : subtreeObjectIDs) {
        auto indexIt = index.objectIndexByID.find(objectID);
        if (indexIt == index.objectIndexByID.end()) continue;
        const std::string nodeID = objects[indexIt->second].value("prefabNodeID", std::string{});
        if (!nodeID.empty()) result[nodeID] = objectID;
    }
    return result;
}

std::string ResolveParentNodeID(
    const JSON &object,
    const std::unordered_map<std::string, std::string> &objectToNodeID) {
    LogScope scope;
    const std::string parentObjectID = GetTransformParentID(object);
    auto it = objectToNodeID.find(parentObjectID);
    return it != objectToNodeID.end() ? it->second : std::string{};
}

void SyncOneInstance(
    JSON &objects,
    const std::string &rootObjectID,
    const PrefabGraph &oldGraph,
    const PrefabGraph &newGraph,
    SceneSyncResult &result) {
    LogScope scope;
    SceneIndex sceneIndex = BuildSceneIndex(objects);
    if (!sceneIndex.objectIndexByID.contains(rootObjectID)) return;

    const auto subtreeIDs = CollectSubtreeObjectIDs(sceneIndex, rootObjectID);
    auto liveNodeToObjectID = BuildLiveNodeMap(objects, sceneIndex, subtreeIDs);
    std::unordered_map<std::string, std::string> objectToNodeID;
    for (const auto &[nodeID, objectID] : liveNodeToObjectID) objectToNodeID[objectID] = nodeID;

    ++result.instancesMatched;

    // Prefab側で追加されたノードへ先にobjectIDを割り当てる。既存ノードのコンポーネントが
    // 新規ノードを参照する変更にも、同じ三者マージ内で正しいインスタンスIDを設定できる。
    std::unordered_map<std::string, std::string> addedObjectIDByNodeID;
    for (const auto &newNode : newGraph.nodes) {
        if (oldGraph.byID.contains(newNode.nodeID) || liveNodeToObjectID.contains(newNode.nodeID)) continue;
        addedObjectIDByNodeID[newNode.nodeID] = UUID128(true).ToString();
    }

    std::unordered_map<std::string, std::string> oldObjectIDRemap;
    std::unordered_map<std::string, std::string> newObjectIDRemap;
    for (const auto &oldNode : oldGraph.nodes) {
        auto target = liveNodeToObjectID.find(oldNode.nodeID);
        const std::string sourceID = oldNode.object->value("objectID", std::string{});
        if (target != liveNodeToObjectID.end() && !sourceID.empty()) {
            oldObjectIDRemap[sourceID] = target->second;
        }
    }
    for (const auto &newNode : newGraph.nodes) {
        std::string targetID;
        auto liveTarget = liveNodeToObjectID.find(newNode.nodeID);
        if (liveTarget != liveNodeToObjectID.end()) targetID = liveTarget->second;
        auto addedTarget = addedObjectIDByNodeID.find(newNode.nodeID);
        if (addedTarget != addedObjectIDByNodeID.end()) targetID = addedTarget->second;
        const std::string sourceID = newNode.object->value("objectID", std::string{});
        if (!targetID.empty() && !sourceID.empty()) newObjectIDRemap[sourceID] = targetID;
    }

    // 既存ノードの値は三者比較し、インスタンス側で変更されていないものだけを更新する。
    for (const auto &[nodeID, oldNode] : oldGraph.byID) {
        auto newIt = newGraph.byID.find(nodeID);
        auto liveIt = liveNodeToObjectID.find(nodeID);
        if (newIt == newGraph.byID.end() || liveIt == liveNodeToObjectID.end()) continue;
        auto objectIt = sceneIndex.objectIndexByID.find(liveIt->second);
        if (objectIt == sceneIndex.objectIndexByID.end()) continue;
        JSON &instanceObject = objects[objectIt->second];
        MergeObjectProperties(instanceObject, *oldNode->object, *newIt->second->object, result);
        MergeComponents(instanceObject, *oldNode->object, *newIt->second->object,
            oldObjectIDRemap, newObjectIDRemap, result);
    }

    for (const auto &newNode : newGraph.nodes) {
        auto addedIt = addedObjectIDByNodeID.find(newNode.nodeID);
        if (addedIt == addedObjectIDByNodeID.end()) continue;
        JSON addedObject = *newNode.object;
        PrefabUtility::RemapObjectIDReferences(addedObject, newObjectIDRemap);
        addedObject["objectID"] = addedIt->second;

        std::string parentObjectID;
        if (!newNode.parentNodeID.empty()) {
            auto addedParent = addedObjectIDByNodeID.find(newNode.parentNodeID);
            if (addedParent != addedObjectIDByNodeID.end()) {
                parentObjectID = addedParent->second;
            } else {
                auto liveParent = liveNodeToObjectID.find(newNode.parentNodeID);
                if (liveParent != liveNodeToObjectID.end()) parentObjectID = liveParent->second;
            }
        }
        SetTransformParentID(addedObject, parentObjectID);
        objects.push_back(std::move(addedObject));
        liveNodeToObjectID[newNode.nodeID] = addedIt->second;
        objectToNodeID[addedIt->second] = newNode.nodeID;
        ++result.nodesAdded;
    }

    // 追加ノードを含めた索引を作り直し、Prefab側で行われた親変更を非破壊マージする。
    sceneIndex = BuildSceneIndex(objects);
    for (const auto &[nodeID, oldNode] : oldGraph.byID) {
        auto newIt = newGraph.byID.find(nodeID);
        auto liveIt = liveNodeToObjectID.find(nodeID);
        if (newIt == newGraph.byID.end() || liveIt == liveNodeToObjectID.end()) continue;
        const std::string &oldParentNodeID = oldNode->parentNodeID;
        const std::string &newParentNodeID = newIt->second->parentNodeID;
        if (oldParentNodeID == newParentNodeID) continue;

        auto objectIt = sceneIndex.objectIndexByID.find(liveIt->second);
        if (objectIt == sceneIndex.objectIndexByID.end()) continue;
        JSON &instanceObject = objects[objectIt->second];
        if (ResolveParentNodeID(instanceObject, objectToNodeID) != oldParentNodeID) {
            continue; // 親子関係がローカルOverride済み
        }

        std::string newParentObjectID;
        if (!newParentNodeID.empty()) {
            auto newParentIt = liveNodeToObjectID.find(newParentNodeID);
            if (newParentIt == liveNodeToObjectID.end()) continue;
            newParentObjectID = newParentIt->second;
        }
        SetTransformParentID(instanceObject, newParentObjectID);
        ++result.nodesReparented;
    }

    // Prefab側で削除されたノードを根に、現在もその下にあるローカル追加子孫を含めて削除する。
    sceneIndex = BuildSceneIndex(objects);
    std::unordered_set<std::string> removedRootObjectIDs;
    for (const auto &[nodeID, oldNode] : oldGraph.byID) {
        if (newGraph.byID.contains(nodeID)) continue;
        auto liveIt = liveNodeToObjectID.find(nodeID);
        if (liveIt != liveNodeToObjectID.end()) removedRootObjectIDs.insert(liveIt->second);
    }

    std::unordered_set<std::string> objectIDsToRemove;
    for (const auto &removedRootID : removedRootObjectIDs) {
        const auto removedSubtree = CollectSubtreeObjectIDs(sceneIndex, removedRootID);
        objectIDsToRemove.insert(removedSubtree.begin(), removedSubtree.end());
    }
    if (!objectIDsToRemove.empty()) {
        JSON keptObjects = JSON::array();
        for (auto &object : objects) {
            const std::string objectID = object.value("objectID", std::string{});
            if (objectIDsToRemove.contains(objectID)) {
                ++result.nodesRemoved;
            } else {
                keptObjects.push_back(std::move(object));
            }
        }
        objects = std::move(keptObjects);
    }
}

} // namespace

SceneSyncResult SyncSceneJson(
    JSON &sceneJson,
    const UUID128 &prefabID,
    const JSON &oldPrefabJson,
    const JSON &newPrefabJson) {
    LogScope scope;
    SceneSyncResult result;
    if (!prefabID.IsValid() || !sceneJson.is_object()) return result;
    if (!sceneJson.contains("sceneObjects") || !sceneJson["sceneObjects"].is_array()) return result;

    const PrefabGraph oldGraph = BuildPrefabGraph(oldPrefabJson);
    const PrefabGraph newGraph = BuildPrefabGraph(newPrefabJson);
    if (oldGraph.nodes.empty() && newGraph.nodes.empty()) return result;

    JSON &objects = sceneJson["sceneObjects"];
    std::vector<std::string> instanceRootIDs;
    for (const auto &object : objects) {
        if (!HasPrefabMarker(object, prefabID)) continue;
        const std::string objectID = object.value("objectID", std::string{});
        if (!objectID.empty()) instanceRootIDs.push_back(objectID);
    }
    for (const auto &rootObjectID : instanceRootIDs) {
        SyncOneInstance(objects, rootObjectID, oldGraph, newGraph, result);
    }
    return result;
}

} // namespace PrefabSceneSync
} // namespace KashipanEngine

#endif // USE_IMGUI
