#include "PrefabUtility.h"
#ifdef USE_IMGUI

#include <unordered_map>

#include "Debug/Logger.h"
#include "Objects/Components/Transform.h"
#include "Objects/EmptyObject.h"
#include "Scene/SceneEditorContext.h"
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {
namespace PrefabUtility {

namespace {
/// @brief オブジェクトJSON内のTransformの親参照（customData["parent"]）を消去する
void EraseTransformParent(JSON &objectJson) {
    LogScope scope;
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
    LogScope scope;
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

void RemapObjectIDReferences(JSON &value, const std::unordered_map<std::string, std::string> &remap) {
    LogScope scope;
    if (value.is_string()) {
        const std::string current = value.get<std::string>();
        auto it = remap.find(current);
        if (it != remap.end()) value = it->second;
        return;
    }
    if (value.is_array()) {
        for (auto &element : value) RemapObjectIDReferences(element, remap);
        return;
    }
    if (value.is_object()) {
        for (auto &entry : value.items()) RemapObjectIDReferences(entry.value(), remap);
    }
}

void CollectSubtreeNodes(SceneEditorContext *context, EmptyObject *obj, int parentIndex,
    std::vector<PasteObjectCommand::Node> &out) {
    LogScope scope;
    if (!obj || !context) return;
    const int myIndex = static_cast<int>(out.size());
    out.push_back({ context->SaveObjectToJson(obj), parentIndex });

    // 子オブジェクトをTransformの親参照から探す
    for (auto *candidate : context->GetSceneObjects()) {
        if (!candidate || candidate == obj) continue;
        auto *candidateTransform = candidate->GetComponent<Transform>();
        if (candidateTransform && candidateTransform->GetParentObject() == obj) {
            CollectSubtreeNodes(context, candidate, myIndex, out);
        }
    }
}

JSON BuildPrefabJson(SceneEditorContext *context, EmptyObject *rootObject, const UUID128 &prefabID) {
    LogScope scope;
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
    LogScope scope;
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
    LogScope scope;
    std::vector<PasteObjectCommand::Node> result = source;

    std::vector<UUID128> freshIDs;
    freshIDs.reserve(result.size());
    for (size_t i = 0; i < result.size(); ++i) {
        freshIDs.emplace_back(true);
    }

    // 旧objectID→新objectIDの対応表を作り、部分木内部の相互参照（Animator.rootBoneObjectID、
    // TargetLookAt.targetObjectID、ParticleSystemの各種参照、RendererのtargetObjectID等）を
    // まとめて新IDへ張り替える。部分木の外側を指す参照は対応表に無いため変更されない
    std::unordered_map<std::string, std::string> objectIDRemap;
    objectIDRemap.reserve(result.size());
    for (size_t i = 0; i < result.size(); ++i) {
        const std::string oldID = result[i].json.value("objectID", std::string{});
        if (!oldID.empty()) objectIDRemap[oldID] = freshIDs[i].ToString();
    }

    for (size_t i = 0; i < result.size(); ++i) {
        JSON &json = result[i].json;
        json["objectID"] = freshIDs[i].ToString();
        if (!json.contains("components")) continue;
        RemapObjectIDReferences(json["components"], objectIDRemap);
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

void OffsetRootsToWorldPosition(std::vector<PasteObjectCommand::Node> &nodes, const Vector3 &worldPosition) {
    LogScope scope;
    // 差分適用の基準として、最初のルートノードの元の位置を探す
    bool hasOrigin = false;
    Vector3 origin{};
    for (auto &node : nodes) {
        if (node.parentIndexInSubtree >= 0 || !node.json.contains("components")) continue;
        for (auto &compJson : node.json["components"]) {
            if (compJson.value("type", "") != "Transform" || !compJson.contains("data")) continue;
            auto &data = compJson["data"];
            if (!data.contains("customData") || !data["customData"].contains("translate")) continue;
            origin = FromJSON<Vector3>(data["customData"]["translate"]);
            hasOrigin = true;
            break;
        }
        if (hasOrigin) break;
    }
    if (!hasOrigin) return;

    const Vector3 delta = worldPosition - origin;
    for (auto &node : nodes) {
        if (node.parentIndexInSubtree >= 0 || !node.json.contains("components")) continue;
        for (auto &compJson : node.json["components"]) {
            if (compJson.value("type", "") != "Transform" || !compJson.contains("data")) continue;
            auto &data = compJson["data"];
            if (!data.contains("customData") || !data["customData"].contains("translate")) continue;
            auto &customData = data["customData"];
            customData["translate"] = ToJSON(FromJSON<Vector3>(customData["translate"]) + delta);
        }
    }
}

std::vector<GhostPreviewMesh> BuildGhostPreviewMeshes(
    const std::vector<PasteObjectCommand::Node> &sourceNodes, const Vector3 &worldPosition) {
    LogScope scope;
    std::vector<GhostPreviewMesh> result;
    if (sourceNodes.empty()) return result;

    // OffsetRootsToWorldPositionと同じ規約で、ルートをworldPositionへ移動した状態のノード列を作る
    // （元のsourceNodesは書き換えない）
    std::vector<PasteObjectCommand::Node> nodes = sourceNodes;
    OffsetRootsToWorldPosition(nodes, worldPosition);

    // 各ノードのワールド行列は、parentIndexInSubtree（常に自分より前のインデックスを指す。
    // CollectSubtreeNodesがpre-orderで積んでいくため）を辿って親から順に確定させていく
    std::vector<Matrix4x4> worldMatrices(nodes.size(), Matrix4x4::Identity());
    for (size_t i = 0; i < nodes.size(); ++i) {
        const JSON &objectJson = nodes[i].json;

        Vector3 translate{ 0.0f, 0.0f, 0.0f };
        Quaternion rotate = Quaternion::Identity();
        Vector3 scale{ 1.0f, 1.0f, 1.0f };
        ModelManager::ModelHandle meshHandle = ModelManager::kInvalidHandle;
        bool hasActiveMeshRenderer = false;

        if (objectJson.contains("components")) {
            for (const auto &compJson : objectJson["components"]) {
                if (!compJson.contains("data")) continue;
                const auto &data = compJson["data"];
                if (!data.value("isActive", true) || !data.contains("customData")) continue;
                const auto &customData = data["customData"];
                const std::string type = compJson.value("type", "");

                if (type == "Transform") {
                    if (customData.contains("translate")) translate = FromJSON<Vector3>(customData["translate"]);
                    if (customData.contains("rotate")) rotate = FromJSON<Quaternion>(customData["rotate"]);
                    if (customData.contains("scale")) scale = FromJSON<Vector3>(customData["scale"]);
                } else if (type == "MeshFilter") {
                    const std::string assetPath = customData.value("meshAssetPath", std::string());
                    if (!assetPath.empty()) meshHandle = ModelManager::GetModelHandleFromAssetPath(assetPath);
                } else if (type == "MeshRenderer") {
                    hasActiveMeshRenderer = true;
                }
            }
        }

        // Transform::GetWorldMatrixと同じ合成順（scale*rotate*translate、行ベクトル規約）に揃える
        Matrix4x4 scaleMat;
        scaleMat.MakeScale(scale);
        const Matrix4x4 rotateMat = rotate.MakeRotateMatrix();
        Matrix4x4 translateMat;
        translateMat.MakeTranslate(translate);
        const Matrix4x4 local = scaleMat * rotateMat * translateMat;

        const int parentIndex = nodes[i].parentIndexInSubtree;
        const Matrix4x4 world = (parentIndex >= 0 && static_cast<size_t>(parentIndex) < i)
            ? local * worldMatrices[static_cast<size_t>(parentIndex)]
            : local;
        worldMatrices[i] = world;

        const bool objectActive = objectJson.value("isActive", true);
        if (objectActive && hasActiveMeshRenderer && meshHandle != ModelManager::kInvalidHandle) {
            result.push_back({ meshHandle, world });
        }
    }
    return result;
}

} // namespace PrefabUtility
} // namespace KashipanEngine

#endif // USE_IMGUI
