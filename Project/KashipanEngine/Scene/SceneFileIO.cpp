#include "SceneFileIO.h"
#include "Utilities/Conversion/ConvertString.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace KashipanEngine {

namespace {

constexpr const char *kSceneFolderSuffix = ".scene";
constexpr const char *kSceneManifestFileName = "/scene.json";
constexpr const char *kObjectsDirName = "/Objects";
constexpr const char *kObjectFileName = "/object.json";

bool IsFolderFormatPath(const std::string &path) {
    return path.ends_with(kSceneFolderSuffix);
}

/// @brief オブジェクトJSON（EmptyObject::SaveToJsonの出力）からTransformコンポーネントの
///        親オブジェクトのUUID文字列を取得する（見つからない場合は空文字＝ルート扱い）
/// @details PrefabUtility.cppのEraseTransformParentと同じ探索経路（components→Transform→
///          data→customData→parent）を読み取るだけで、書き換えは行わない
std::string ExtractParentID(const JSON &objectJson) {
    if (!objectJson.contains("components")) return std::string();
    for (const auto &compJson : objectJson["components"]) {
        if (compJson.value("type", "") != "Transform") continue;
        if (!compJson.contains("data")) return std::string();
        const auto &data = compJson["data"];
        if (!data.contains("customData")) return std::string();
        const auto &customData = data["customData"];
        if (!customData.contains("parent")) return std::string();
        return customData["parent"].get<std::string>();
    }
    return std::string();
}

bool SaveSceneFolder(const std::string &path, const JSON &sceneJson) {
    // マニフェスト（シーンレベルのメタ情報のみ。オブジェクト順序は含めない）
    JSON manifest;
    manifest["sceneName"] = sceneJson.value("sceneName", "");
    manifest["sceneID"] = sceneJson.value("sceneID", "");
    manifest["sceneComponents"] = sceneJson.value("sceneComponents", JSON::array());
    manifest["sceneVariables"] = sceneJson.value("sceneVariables", JSON::array());
    if (!SaveJSON(manifest, path + kSceneManifestFileName)) return false;

    const auto &objects = sceneJson.value("sceneObjects", JSON::array());

    // 現在シーンに実在するオブジェクトID集合（親が見つかるかどうかの判定用）
    std::unordered_set<std::string> objectIDs;
    for (const auto &objJson : objects) {
        objectIDs.insert(objJson.value("objectID", ""));
    }

    // 親UUID（ルートは空文字）→子オブジェクトの配列インデックス列（元の配列順のまま）
    std::unordered_map<std::string, std::vector<size_t>> childrenByParent;
    for (size_t i = 0; i < objects.size(); ++i) {
        std::string parentID = ExtractParentID(objects[i]);
        if (!parentID.empty() && !objectIDs.contains(parentID)) {
            parentID.clear(); // 親が見つからない場合はルート扱い
        }
        childrenByParent[parentID].push_back(i);
    }

    const std::string objectsDir = path + kObjectsDirName;

    // 保存の都度Objects/を丸ごと作り直す。nlohmann::jsonはキーをアルファベット順にdumpするため
    // 内容が同じオブジェクトは毎回バイト単位で同一のファイルになり、Git上は実際に変更された
    // オブジェクトのみが差分として現れる（削除済みオブジェクトのフォルダも確実に消える）
    std::error_code ec;
    std::filesystem::remove_all(Utf8StringToPath(objectsDir), ec);

    for (const auto &[parentID, indices] : childrenByParent) {
        for (size_t siblingIndex = 0; siblingIndex < indices.size(); ++siblingIndex) {
            const JSON &objJson = objects[indices[siblingIndex]];
            const std::string objectID = objJson.value("objectID", "");
            if (objectID.empty()) continue;
            JSON wrapped;
            wrapped["siblingIndex"] = static_cast<int>(siblingIndex);
            wrapped["object"] = objJson;
            SaveJSON(wrapped, objectsDir + "/" + objectID + kObjectFileName);
        }
    }
    return true;
}

JSON LoadSceneFolder(const std::string &path) {
    JSON manifest = LoadJSON(path + kSceneManifestFileName);
    if (manifest.empty()) return JSON();

    JSON sceneJson;
    sceneJson["sceneName"] = manifest.value("sceneName", "");
    sceneJson["sceneID"] = manifest.value("sceneID", "");
    sceneJson["sceneComponents"] = manifest.value("sceneComponents", JSON::array());
    sceneJson["sceneVariables"] = manifest.value("sceneVariables", JSON::array());

    struct LoadedObject {
        int siblingIndex = 0;
        JSON objectJson;
        std::string parentID;
    };
    std::unordered_map<std::string, LoadedObject> objectsByID;

    std::error_code ec;
    const std::filesystem::path objectsPath = Utf8StringToPath(path + kObjectsDirName);
    if (std::filesystem::exists(objectsPath, ec) && std::filesystem::is_directory(objectsPath, ec)) {
        for (const auto &entry : std::filesystem::directory_iterator(objectsPath, ec)) {
            if (!entry.is_directory()) continue;
            JSON wrapped = LoadJSON(PathToUtf8String(entry.path()) + kObjectFileName);
            if (wrapped.empty() || !wrapped.contains("object")) continue;
            JSON objJson = wrapped["object"];
            const std::string objectID = objJson.value("objectID", "");
            if (objectID.empty()) continue;
            LoadedObject loaded;
            loaded.siblingIndex = wrapped.value("siblingIndex", 0);
            loaded.parentID = ExtractParentID(objJson);
            loaded.objectJson = std::move(objJson);
            objectsByID.emplace(objectID, std::move(loaded));
        }
    }

    // 親ID（ルートは空文字）→子IDリストをsiblingIndex順に構築
    std::unordered_map<std::string, std::vector<std::string>> childrenByParent;
    for (const auto &[objectID, loaded] : objectsByID) {
        std::string parentID = loaded.parentID;
        if (!parentID.empty() && !objectsByID.contains(parentID)) {
            parentID.clear(); // 親が見つからない場合はルート扱い
        }
        childrenByParent[parentID].push_back(objectID);
    }
    for (auto &[parentID, ids] : childrenByParent) {
        std::sort(ids.begin(), ids.end(), [&objectsByID](const std::string &a, const std::string &b) {
            return objectsByID.at(a).siblingIndex < objectsByID.at(b).siblingIndex;
        });
    }

    // ルートから深さ優先でフラットな配列へ組み立てる（同じ親を持つオブジェクト同士の
    // 相対順序さえ保たれていればヒエラルキー表示順は正しく復元される）
    JSON sceneObjects = JSON::array();
    std::function<void(const std::string &)> appendSubtree = [&](const std::string &objectID) {
        sceneObjects.push_back(objectsByID.at(objectID).objectJson);
        auto it = childrenByParent.find(objectID);
        if (it == childrenByParent.end()) return;
        for (const auto &childID : it->second) {
            appendSubtree(childID);
        }
    };
    auto rootIt = childrenByParent.find(std::string());
    if (rootIt != childrenByParent.end()) {
        for (const auto &rootID : rootIt->second) {
            appendSubtree(rootID);
        }
    }
    sceneJson["sceneObjects"] = std::move(sceneObjects);
    return sceneJson;
}

} // namespace

bool SaveSceneToPath(const JSON &sceneJson, const std::string &path) {
    if (IsFolderFormatPath(path)) return SaveSceneFolder(path, sceneJson);
    return SaveJSON(sceneJson, path);
}

JSON LoadSceneFromPath(const std::string &path) {
    if (IsFolderFormatPath(path)) return LoadSceneFolder(path);
    return LoadJSON(path);
}

} // namespace KashipanEngine
