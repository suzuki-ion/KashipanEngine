#include "PrefabSyncUtility.h"
#ifdef USE_IMGUI
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "ComponentSerialize/ComponentRegistry.h"
#include "Objects/Components/PrefabInstanceComponent.h"
#include "Objects/Components/Transform.h"
#include "Objects/EmptyObject.h"
#include "Scene/Editor/PrefabAssetManager.h"
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
        entries.emplace_back(entry.first.get(), entry.second);
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
    for (const auto &objPtr : context->GetSceneObjects()) {
        EmptyObject *candidate = objPtr.get();
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

    auto composite = std::make_unique<CompositeCommand>("Revert Prefab Instance: " + name);
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

void SyncOtherInstances(SceneEditorContext *context, SceneEditorCommands *commands,
    const UUID128 &prefabID, const JSON &oldPrefabJson, const JSON &newPrefabJson) {
    if (!context || !prefabID.IsValid()) return;

    const auto oldByID = IndexPrefabObjectsByNodeID(oldPrefabJson);
    const auto newByID = IndexPrefabObjectsByNodeID(newPrefabJson);

    std::vector<EmptyObject *> roots;
    for (const auto &objPtr : context->GetSceneObjects()) {
        EmptyObject *obj = objPtr.get();
        auto *comp = obj ? obj->GetComponent<PrefabInstanceComponent>() : nullptr;
        if (comp && comp->GetPrefabID() == prefabID) roots.push_back(obj);
    }

    for (EmptyObject *root : roots) {
        auto composite = std::make_unique<CompositeCommand>("Apply Prefab: " + root->GetName());
        for (EmptyObject *obj : CollectSubtreeObjects(context, root)) {
            if (!obj->GetPrefabNodeID().IsValid()) continue;
            const std::string nodeIDStr = obj->GetPrefabNodeID().ToString();
            auto oldIt = oldByID.find(nodeIDStr);
            auto newIt = newByID.find(nodeIDStr);
            // 対応ノード自体が旧/新どちらかに無い＝オブジェクトの追加/削除という構造変化は
            // 自動伝播しない（対応するインスタンスのApply All/Revert Allで個別に対応する）。
            // コンポーネント単位の追加/削除はこの下で扱う
            if (oldIt == oldByID.end() || newIt == newByID.end()) continue;

            const JSON &oldComponents = GetComponentsArray(*oldIt->second);
            const JSON &newComponents = GetComponentsArray(*newIt->second);

            // 型ごとの旧/新/ライブの出現数を数え、型+ordinalの全組み合わせ（追加・削除された分も含む）を走査する
            std::unordered_map<std::string, size_t> oldTypeCount, newTypeCount;
            for (const auto &c : oldComponents) oldTypeCount[c.value("type", "")]++;
            for (const auto &c : newComponents) newTypeCount[c.value("type", "")]++;

            std::unordered_map<std::string, std::vector<IObjectComponent *>> liveByType;
            for (IObjectComponent *liveComp : GetOrderedComponents(obj)) {
                liveByType[liveComp->GetComponentType()].push_back(liveComp);
            }

            std::unordered_set<std::string> allTypes;
            for (const auto &[type, count] : oldTypeCount) allTypes.insert(type);
            for (const auto &[type, count] : newTypeCount) allTypes.insert(type);
            for (const auto &[type, comps] : liveByType) allTypes.insert(type);

            for (const auto &type : allTypes) {
                const size_t oldCount = oldTypeCount.contains(type) ? oldTypeCount[type] : 0;
                const size_t newCount = newTypeCount.contains(type) ? newTypeCount[type] : 0;
                const auto liveIt = liveByType.find(type);
                const size_t liveCount = (liveIt != liveByType.end()) ? liveIt->second.size() : 0;
                const size_t maxOrdinal = std::max({ oldCount, newCount, liveCount });

                for (size_t ordinal = 0; ordinal < maxOrdinal; ++ordinal) {
                    const JSON *oldEntry = FindComponentJsonByTypeOrdinal(oldComponents, type, ordinal);
                    const JSON *newEntry = FindComponentJsonByTypeOrdinal(newComponents, type, ordinal);
                    IObjectComponent *liveComp = (liveIt != liveByType.end() && ordinal < liveIt->second.size())
                        ? liveIt->second[ordinal] : nullptr;

                    if (oldEntry && newEntry) {
                        // 値の変更（既存の型+ordinal）
                        if (*oldEntry == *newEntry) continue; // 値の変更なし
                        if (!liveComp) continue; // ローカルで既に削除済み＝触らない
                        const JSON currentJson = obj->SaveComponentToJson(liveComp);
                        if (currentJson != *oldEntry) continue; // ローカルでOverride済み＝尊重して何もしない
                        composite->AddCommand(std::make_unique<ComponentEditCommand>(obj, liveComp, currentJson, *newEntry));
                    } else if (!oldEntry && newEntry) {
                        // Prefab側で新規追加されたコンポーネント
                        if (liveComp) continue; // 同じ型+ordinalがローカルに既にある（ローカル追加）＝触らない
                        composite->AddCommand(std::make_unique<AddComponentCommand>(obj, type, *newEntry));
                    } else if (oldEntry && !newEntry) {
                        // Prefab側で削除されたコンポーネント
                        if (!liveComp) continue; // 既にローカルで削除済み
                        const JSON currentJson = obj->SaveComponentToJson(liveComp);
                        if (currentJson != *oldEntry) continue; // ローカルで変更済み＝尊重して残す
                        composite->AddCommand(std::make_unique<RemoveComponentCommand>(obj, liveComp));
                    }
                    // oldEntry・newEntryどちらも無い＝Prefabと無関係のローカル追加コンポーネントのため対象外
                }
            }
        }
        if (!composite->IsEmpty() && commands) {
            commands->Execute(std::move(composite));
        }
    }
}

} // namespace PrefabSyncUtility
} // namespace KashipanEngine
#endif // USE_IMGUI
