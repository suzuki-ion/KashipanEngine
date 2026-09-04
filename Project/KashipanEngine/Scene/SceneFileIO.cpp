#include "SceneFileIO.h"
#include "Core/ProjectPaths.h"
#include "Debug/Logger.h"
#include "Utilities/Conversion/ConvertString.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace KashipanEngine {

namespace {

constexpr const char *kSceneFolderSuffix = ".scene";
constexpr const char *kSceneManifestFileName = "/scene.json";
constexpr const char *kObjectsDirName = "/Objects";
constexpr const char *kObjectFileName = "/object.json";
// siblingIndexを割り振る際の間隔。1つのオブジェクトを移動・追加しただけであれば、
// 前後のオブジェクトの値の間に新しい値を割り込ませるだけで済み、他の兄弟の
// siblingIndexを書き換えずに済む（＝Gitでの差分・コンフリクトを局所化できる）
constexpr long long kSiblingIndexGap = 1000;

bool IsFolderFormatPath(const std::string &path) {
    LogScope scope;
    return path.ends_with(kSceneFolderSuffix);
}

/// @brief オブジェクトJSON（EmptyObject::SaveToJsonの出力）からTransformコンポーネントの
///        親オブジェクトのUUID文字列を取得する（見つからない場合は空文字＝ルート扱い）
/// @details PrefabUtility.cppのEraseTransformParentと同じ探索経路（components→Transform→
///          data→customData→parent）を読み取るだけで、書き換えは行わない
std::string ExtractParentID(const JSON &objectJson) {
    LogScope scope;
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

/// @brief 1つの親を持つ兄弟オブジェクト列に対して、新しいsiblingIndexを決定する
/// @param oldValues 各兄弟の直前セーブ時のsiblingIndex（同じ親のまま存在していた場合のみ値あり）
/// @details 現在の並び順（引数の配列順）を崩さない範囲で、旧値をそのまま使える最長の部分列を
///          最長増加部分列（LIS）で求め、その部分列に含まれるオブジェクトは値を変更しない。
///          それ以外（新規追加・移動されたオブジェクト）だけに、前後の確定値の間へ収まる新しい
///          値を割り当てる。間隔が枯渇している場合のみ、そのグループ全体を等間隔で振り直す
std::vector<long long> AssignSiblingIndices(const std::vector<std::optional<long long>> &oldValues) {
    LogScope scope;
    const size_t count = oldValues.size();
    std::vector<long long> finalValues(count);
    if (count == 0) return finalValues;

    // 旧値を持つ要素だけを対象にLISを求め、維持できる値を確定させる
    std::vector<std::optional<long long>> keep(count);
    {
        std::vector<size_t> withValueIdx;
        std::vector<long long> values;
        for (size_t i = 0; i < count; ++i) {
            if (oldValues[i].has_value()) {
                withValueIdx.push_back(i);
                values.push_back(*oldValues[i]);
            }
        }
        const size_t n = values.size();
        std::vector<int> lisLength(n, 1);
        std::vector<int> lisPrev(n, -1);
        int bestEnd = -1, bestLen = 0;
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < i; ++j) {
                if (values[j] < values[i] && lisLength[j] + 1 > lisLength[i]) {
                    lisLength[i] = lisLength[j] + 1;
                    lisPrev[i] = static_cast<int>(j);
                }
            }
            if (lisLength[i] > bestLen) {
                bestLen = lisLength[i];
                bestEnd = static_cast<int>(i);
            }
        }
        for (int cur = bestEnd; cur != -1; cur = lisPrev[cur]) {
            keep[withValueIdx[static_cast<size_t>(cur)]] = values[static_cast<size_t>(cur)];
        }
    }

    bool rebalanceAll = false;
    for (size_t i = 0; i < count && !rebalanceAll;) {
        if (keep[i].has_value()) {
            finalValues[i] = *keep[i];
            ++i;
            continue;
        }
        const size_t runStart = i;
        while (i < count && !keep[i].has_value()) ++i;
        const size_t runLen = i - runStart;
        const std::optional<long long> prevValue = (runStart > 0) ? keep[runStart - 1] : std::nullopt;
        const std::optional<long long> nextValue = (i < count) ? keep[i] : std::nullopt;

        if (prevValue && nextValue) {
            const long long span = *nextValue - *prevValue;
            if (span <= static_cast<long long>(runLen)) {
                rebalanceAll = true;
                break;
            }
            for (size_t k = 0; k < runLen; ++k) {
                finalValues[runStart + k] = *prevValue + span * static_cast<long long>(k + 1) / static_cast<long long>(runLen + 1);
            }
        } else if (prevValue && !nextValue) {
            for (size_t k = 0; k < runLen; ++k) {
                finalValues[runStart + k] = *prevValue + kSiblingIndexGap * static_cast<long long>(k + 1);
            }
        } else if (!prevValue && nextValue) {
            for (size_t k = 0; k < runLen; ++k) {
                finalValues[runStart + k] = *nextValue - kSiblingIndexGap * static_cast<long long>(runLen - k);
            }
        } else {
            for (size_t k = 0; k < runLen; ++k) {
                finalValues[runStart + k] = kSiblingIndexGap * static_cast<long long>(k);
            }
        }
    }

    if (rebalanceAll) {
        for (size_t i = 0; i < count; ++i) {
            finalValues[i] = kSiblingIndexGap * static_cast<long long>(i);
        }
    }
    return finalValues;
}

bool SaveSceneFolder(const std::string &path, const JSON &sceneJson) {
    LogScope scope;
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

    // 兄弟順を維持したまま移動していないオブジェクトのsiblingIndexを流用するため、
    // 削除前に前回保存時の（親ID, siblingIndex）を読み取っておく
    struct OldSiblingInfo {
        std::string parentID;
        long long siblingIndex;
    };
    std::unordered_map<std::string, OldSiblingInfo> oldInfoByID;
    {
        std::error_code readEc;
        const auto objectsPath = Utf8StringToPath(objectsDir);
        if (std::filesystem::exists(objectsPath, readEc) && std::filesystem::is_directory(objectsPath, readEc)) {
            for (const auto &entry : std::filesystem::directory_iterator(objectsPath, readEc)) {
                if (!entry.is_directory()) continue;
                JSON wrapped = LoadJSON(PathToUtf8String(entry.path()) + kObjectFileName);
                if (wrapped.empty() || !wrapped.contains("object")) continue;
                const JSON &oldObjJson = wrapped["object"];
                const std::string objectID = oldObjJson.value("objectID", "");
                if (objectID.empty()) continue;
                oldInfoByID[objectID] = OldSiblingInfo{ExtractParentID(oldObjJson), wrapped.value("siblingIndex", 0)};
            }
        }
    }

    // 保存の都度Objects/を丸ごと作り直す。nlohmann::jsonはキーをアルファベット順にdumpするため
    // 内容が同じオブジェクトは毎回バイト単位で同一のファイルになり、Git上は実際に変更された
    // オブジェクトのみが差分として現れる（削除済みオブジェクトのフォルダも確実に消える）
    std::error_code ec;
    std::filesystem::remove_all(Utf8StringToPath(objectsDir), ec);

    for (const auto &[parentID, indices] : childrenByParent) {
        std::vector<std::optional<long long>> oldValues(indices.size());
        for (size_t i = 0; i < indices.size(); ++i) {
            const std::string objectID = objects[indices[i]].value("objectID", "");
            const auto it = oldInfoByID.find(objectID);
            // 親が変わっていなければ、前回保存時のsiblingIndexを維持候補として使う
            if (it != oldInfoByID.end() && it->second.parentID == parentID) {
                oldValues[i] = it->second.siblingIndex;
            }
        }
        const std::vector<long long> finalValues = AssignSiblingIndices(oldValues);

        for (size_t i = 0; i < indices.size(); ++i) {
            const JSON &objJson = objects[indices[i]];
            const std::string objectID = objJson.value("objectID", "");
            if (objectID.empty()) continue;
            JSON wrapped;
            wrapped["siblingIndex"] = static_cast<int>(finalValues[i]);
            wrapped["object"] = objJson;
            SaveJSON(wrapped, objectsDir + "/" + objectID + kObjectFileName);
        }
    }
    return true;
}

JSON LoadSceneFolder(const std::string &path) {
    LogScope scope;
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
    LogScope scope;
    // 呼び出し元は "Assets/Scenes/..." や "SceneBackups/..." といった論理パスを渡してくるため、
    // ここで開いているプロジェクト基準の物理パスへ変換する
    const std::string resolvedPath = ProjectPaths::ToPhysical(path);
    if (IsFolderFormatPath(resolvedPath)) return SaveSceneFolder(resolvedPath, sceneJson);
    return SaveJSON(sceneJson, resolvedPath);
}

JSON LoadSceneFromPath(const std::string &path) {
    LogScope scope;
    const std::string resolvedPath = ProjectPaths::ToPhysical(path);
    if (IsFolderFormatPath(resolvedPath)) return LoadSceneFolder(resolvedPath);
    return LoadJSON(resolvedPath);
}

} // namespace KashipanEngine
