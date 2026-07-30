#include "PrefabUtility.h"
#ifdef USE_IMGUI

#include "Objects/Components/Transform.h"
#include "Objects/EmptyObject.h"
#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {
namespace PrefabUtility {

namespace {
/// @brief オブジェクトJSON内のTransformの親参照（customData["parent"]）を消去する
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
/// @details Prefab化する対象が既に別のPrefabインスタンスの根だった場合、新しく作るPrefabへ
///          古いリンク情報が紛れ込まないようにするため
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
} // namespace

void CollectSubtreeNodes(SceneEditorContext *context, EmptyObject *obj, int parentIndex,
    std::vector<PasteObjectCommand::Node> &out) {
    if (!obj || !context) return;
    const int myIndex = static_cast<int>(out.size());
    out.push_back({ context->SaveObjectToJson(obj), parentIndex });

    // 子オブジェクトをTransformの親参照から探す
    for (const auto &objPtr : context->GetSceneObjects()) {
        EmptyObject *candidate = objPtr.get();
        if (!candidate || candidate == obj) continue;
        auto *candidateTransform = candidate->GetComponent<Transform>();
        if (candidateTransform && candidateTransform->GetParentObject() == obj) {
            CollectSubtreeNodes(context, candidate, myIndex, out);
        }
    }
}

JSON BuildPrefabJson(SceneEditorContext *context, EmptyObject *rootObject, const UUID128 &prefabID) {
    JSON json = JSON::object();
    if (!context || !rootObject) return json;

    std::vector<PasteObjectCommand::Node> nodes;
    CollectSubtreeNodes(context, rootObject, -1, nodes);
    if (nodes.empty()) return json;

    json["prefab"] = true;
    json["prefabID"] = prefabID.ToString();
    json["name"] = rootObject->GetName();
    json["objects"] = JSON::array();
    for (auto &node : nodes) {
        // このノードに対応するライブオブジェクトのprefabNodeIDが未採番なら新規発行し、
        // JSON・ライブオブジェクトの双方へ書き戻す（Prefabとの対応付けに使う安定ID）
        if (EmptyObject *liveObj = context->GetSceneObject(UUID128(node.json.value("objectID", std::string{})))) {
            if (!liveObj->GetPrefabNodeID().IsValid()) {
                const UUID128 newNodeID(true);
                liveObj->SetPrefabNodeID(newNodeID);
                node.json["prefabNodeID"] = newNodeID.ToString();
            }
        }
        // 根オブジェクトの親参照はシーン固有のIDのため、プレハブには残さない
        if (node.parentIndexInSubtree < 0) {
            EraseTransformParent(node.json);
            EraseRootPrefabInstanceComponent(node.json);
        }
        JSON entry;
        entry["parentIndex"] = node.parentIndexInSubtree;
        entry["object"] = node.json;
        json["objects"].push_back(std::move(entry));
    }
    return json;
}

std::vector<PasteObjectCommand::Node> LoadPrefabNodes(const JSON &prefabJson) {
    std::vector<PasteObjectCommand::Node> nodes;
    if (!prefabJson.is_object()) return nodes;

    if (prefabJson.contains("objects") && prefabJson["objects"].is_array()) {
        for (const auto &entry : prefabJson["objects"]) {
            if (!entry.is_object()) continue;
            PasteObjectCommand::Node node;
            node.parentIndexInSubtree = entry.value("parentIndex", -1);
            node.json = entry.value("object", JSON::object());
            if (node.json.empty()) continue;
            nodes.push_back(std::move(node));
        }
    } else if (prefabJson.contains("name")) {
        // オブジェクト保存形式そのままの単一オブジェクトJSONも受け付ける
        nodes.push_back({ prefabJson, -1 });
    }
    return nodes;
}

std::vector<PasteObjectCommand::Node> PrepareNodesForInstantiation(
    const std::vector<PasteObjectCommand::Node> &source, bool preserveRootParent) {
    std::vector<PasteObjectCommand::Node> result = source;

    std::vector<UUID128> freshIDs;
    freshIDs.reserve(result.size());
    for (size_t i = 0; i < result.size(); ++i) {
        freshIDs.emplace_back(true);
    }

    for (size_t i = 0; i < result.size(); ++i) {
        JSON &json = result[i].json;
        json["objectID"] = freshIDs[i].ToString();
        if (!json.contains("components")) continue;
        for (auto &compJson : json["components"]) {
            if (compJson.value("type", "") != "Transform" || !compJson.contains("data")) continue;
            // Transformの実データはSaveToJsonInterfaceにより data["customData"] 以下（parent/translate等）に
            // 格納される（dataの直下ではない）ため、customData側を書き換える必要がある
            auto &data = compJson["data"];
            if (!data.contains("customData")) continue;
            auto &customData = data["customData"];
            const int parentIndex = result[i].parentIndexInSubtree;
            if (parentIndex >= 0 && static_cast<size_t>(parentIndex) < freshIDs.size()) {
                customData["parent"] = freshIDs[static_cast<size_t>(parentIndex)].ToString();
            } else if (!preserveRootParent) {
                customData.erase("parent");
            }
        }
    }
    return result;
}

} // namespace PrefabUtility
} // namespace KashipanEngine

#endif // USE_IMGUI
