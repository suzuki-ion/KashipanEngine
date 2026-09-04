#include "PrefabAssetManager.h"
#ifdef USE_IMGUI
#include "Core/ProjectPaths.h"
#include "Debug/Logger.h"
#include "Scene/Editor/PrefabUtility.h"
#include "Utilities/FileIO/Directory.h"

namespace KashipanEngine {

std::string PrefabAssetManager::GetPrefabPath(const UUID128 &prefabID) {
    LogScope scope;
    EnsureIndexBuilt();
    auto it = sIDToPath_.find(prefabID);
    return it != sIDToPath_.end() ? it->second : std::string{};
}

UUID128 PrefabAssetManager::GetPrefabIDFromPath(const std::string &filePath) {
    LogScope scope;
    EnsureIndexBuilt();
    auto it = sPathToID_.find(filePath);
    return it != sPathToID_.end() ? it->second : UUID128();
}

bool PrefabAssetManager::CreatePrefabFile(const UUID128 &prefabID, const JSON &json, const std::string &filePath) {
    LogScope scope;
    EnsureIndexBuilt();
    if (!prefabID.IsValid()) return false;

    // 既に登録済みのprefabID（モデル再インポート時のスキーマ更新等）を上書きする場合は
    // 変更前のJSONを控えておき、保存後にリスナーへ通知してシーン上の配置済みインスタンスへ
    // 反映する（SavePrefabJsonと同じ経路を通さないと「元Prefabが書き変わればシーン上の
    // インスタンスも書き変わる」同期が一切発生しない）
    const bool hadExisting = sIDToPath_.contains(prefabID);
    const JSON oldJson = hadExisting ? LoadPrefabJson(prefabID) : JSON();

    if (!SaveJSON(json, ProjectPaths::ToPhysical(filePath))) return false;
    sIDToPath_[prefabID] = filePath;
    sPathToID_[filePath] = prefabID;
    if (hadExisting) {
        NotifyChanged(prefabID, oldJson, json);
    } else {
        sCache_[prefabID] = json;
    }
    return true;
}

JSON PrefabAssetManager::LoadPrefabJson(const UUID128 &prefabID) {
    LogScope scope;
    return LoadPrefabJsonRef(prefabID);
}

const JSON &PrefabAssetManager::LoadPrefabJsonRef(const UUID128 &prefabID) {
    LogScope scope;
    static const JSON kEmpty = JSON();
    EnsureIndexBuilt();
    auto cacheIt = sCache_.find(prefabID);
    if (cacheIt != sCache_.end()) return cacheIt->second;

    auto pathIt = sIDToPath_.find(prefabID);
    if (pathIt == sIDToPath_.end()) return kEmpty;
    JSON json = LoadJSON(ProjectPaths::ToPhysical(pathIt->second));
    return (sCache_[prefabID] = std::move(json));
}

bool PrefabAssetManager::SavePrefabJson(const UUID128 &prefabID, const JSON &newJson) {
    LogScope scope;
    EnsureIndexBuilt();
    auto pathIt = sIDToPath_.find(prefabID);
    if (pathIt == sIDToPath_.end()) return false;
    if (!SaveJSON(newJson, ProjectPaths::ToPhysical(pathIt->second))) return false;

    const JSON oldJson = LoadPrefabJson(prefabID);
    NotifyChanged(prefabID, oldJson, newJson);
    return true;
}

bool PrefabAssetManager::SavePrefabJsonByPath(const std::string &filePath, const JSON &newJson) {
    LogScope scope;
    EnsureIndexBuilt();
    const UUID128 prefabID(newJson.value("prefabID", std::string{}));
    if (!prefabID.IsValid()) {
        // prefabIDを持たない通常のJSONファイル（.prefabでも未登録・壊れている場合）はそのまま保存するだけ
        return SaveJSON(newJson, ProjectPaths::ToPhysical(filePath));
    }
    sIDToPath_[prefabID] = filePath;
    sPathToID_[filePath] = prefabID;
    return SavePrefabJson(prefabID, newJson);
}

bool PrefabAssetManager::RenamePrefabFile(const std::string &oldFilePath, const std::string &newFilePath) {
    LogScope scope;
    EnsureIndexBuilt();
    auto it = sPathToID_.find(oldFilePath);
    if (it == sPathToID_.end()) return false;
    const UUID128 prefabID = it->second;
    sPathToID_.erase(it);
    sPathToID_[newFilePath] = prefabID;
    sIDToPath_[prefabID] = newFilePath;
    return true;
}

void PrefabAssetManager::SetChangeListener(ChangeCallback callback) {
    LogScope scope;
    sListener_ = std::move(callback);
}

void PrefabAssetManager::EnsureIndexBuilt() {
    LogScope scope;
    if (sIndexBuilt_) return;
    sIndexBuilt_ = true;

    // インデックスのキーには論理パスを使う（シーンJSONへ保存されるプレハブのパスと揃えるため）
    const std::vector<std::string> files = ProjectPaths::ListAssetFiles({ PrefabUtility::kPrefabExtension });
    for (const auto &filePath : files) {
        JSON json = LoadJSON(ProjectPaths::ToPhysical(filePath));
        const UUID128 prefabID(json.value("prefabID", std::string{}));
        if (!prefabID.IsValid()) continue;
        sIDToPath_[prefabID] = filePath;
        sPathToID_[filePath] = prefabID;
        sCache_[prefabID] = std::move(json);
    }
}

void PrefabAssetManager::NotifyChanged(const UUID128 &prefabID, const JSON &oldJson, const JSON &newJson) {
    LogScope scope;
    sCache_[prefabID] = newJson;
    if (sListener_) sListener_(prefabID, oldJson, newJson);
}

} // namespace KashipanEngine
#endif // USE_IMGUI
