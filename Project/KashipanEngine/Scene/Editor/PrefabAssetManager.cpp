#include "PrefabAssetManager.h"
#ifdef USE_IMGUI
#include <filesystem>
#include "Core/ProjectPaths.h"
#include "Scene/Editor/PrefabUtility.h"
#include "Utilities/FileIO/Directory.h"

namespace KashipanEngine {

std::string PrefabAssetManager::GetPrefabPath(const UUID128 &prefabID) {
    EnsureIndexBuilt();
    auto it = sIDToPath_.find(prefabID);
    return it != sIDToPath_.end() ? it->second : std::string{};
}

UUID128 PrefabAssetManager::GetPrefabIDFromPath(const std::string &filePath) {
    EnsureIndexBuilt();
    auto it = sPathToID_.find(filePath);
    return it != sPathToID_.end() ? it->second : UUID128();
}

bool PrefabAssetManager::CreatePrefabFile(const UUID128 &prefabID, const JSON &json, const std::string &filePath) {
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
    return LoadPrefabJsonRef(prefabID);
}

const JSON &PrefabAssetManager::LoadPrefabJsonRef(const UUID128 &prefabID) {
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
    EnsureIndexBuilt();
    auto pathIt = sIDToPath_.find(prefabID);
    if (pathIt == sIDToPath_.end()) return false;
    if (!SaveJSON(newJson, ProjectPaths::ToPhysical(pathIt->second))) return false;

    const JSON oldJson = LoadPrefabJson(prefabID);
    NotifyChanged(prefabID, oldJson, newJson);
    return true;
}

bool PrefabAssetManager::SavePrefabJsonByPath(const std::string &filePath, const JSON &newJson) {
    EnsureIndexBuilt();
    const UUID128 prefabID(newJson.value("prefabID", std::string{}));
    auto existingPathIt = sPathToID_.find(filePath);
    if (existingPathIt != sPathToID_.end() && existingPathIt->second != prefabID) {
        // 登録済みPrefabからIDを消す編集も、別IDへの変更と同様に既存インスタンスを孤立させる。
        return false;
    }
    if (!prefabID.IsValid()) {
        // prefabIDを持たない通常のJSONファイル（.prefabでも未登録・壊れている場合）はそのまま保存するだけ
        return SaveJSON(newJson, ProjectPaths::ToPhysical(filePath));
    }
    // prefabIDは配置済みインスタンスとの恒久的な対応キー。登録済みファイルのID変更を許すと
    // 旧IDと新IDが同じパスを指し、既存インスタンスが古いキャッシュへ取り残されるため拒否する。
    sIDToPath_[prefabID] = filePath;
    sPathToID_[filePath] = prefabID;
    return SavePrefabJson(prefabID, newJson);
}

bool PrefabAssetManager::RenamePrefabFile(const std::string &oldFilePath, const std::string &newFilePath) {
    EnsureIndexBuilt();
    auto it = sPathToID_.find(oldFilePath);
    if (it == sPathToID_.end()) return false;
    const UUID128 prefabID = it->second;
    sPathToID_.erase(it);
    sPathToID_[newFilePath] = prefabID;
    sIDToPath_[prefabID] = newFilePath;
    return true;
}

void PrefabAssetManager::RenamePrefabFolder(const std::string &oldFolderPath, const std::string &newFolderPath) {
    EnsureIndexBuilt();
    const std::string oldPrefix = oldFolderPath.ends_with('/') ? oldFolderPath : oldFolderPath + "/";
    const std::string newPrefix = newFolderPath.ends_with('/') ? newFolderPath : newFolderPath + "/";
    std::vector<std::pair<std::string, UUID128>> renamed;
    for (const auto &[path, prefabID] : sPathToID_) {
        if (path.starts_with(oldPrefix)) renamed.emplace_back(path, prefabID);
    }
    for (const auto &[oldPath, prefabID] : renamed) {
        const std::string newPath = newPrefix + oldPath.substr(oldPrefix.size());
        sPathToID_.erase(oldPath);
        sPathToID_[newPath] = prefabID;
        sIDToPath_[prefabID] = newPath;
    }
}

void PrefabAssetManager::UnregisterPrefabPath(const std::string &fileOrFolderPath, bool recursive) {
    EnsureIndexBuilt();
    const std::string prefix = fileOrFolderPath.ends_with('/') ? fileOrFolderPath : fileOrFolderPath + "/";
    std::vector<std::pair<std::string, UUID128>> removed;
    for (const auto &[path, prefabID] : sPathToID_) {
        if (path == fileOrFolderPath || (recursive && path.starts_with(prefix))) {
            removed.emplace_back(path, prefabID);
        }
    }
    for (const auto &[path, prefabID] : removed) {
        sPathToID_.erase(path);
        auto idIt = sIDToPath_.find(prefabID);
        if (idIt != sIDToPath_.end() && idIt->second == path) sIDToPath_.erase(idIt);
        sCache_.erase(prefabID);
    }
}

void PrefabAssetManager::ReloadExternallyChangedFiles(const std::vector<std::string> &filePaths) {
    EnsureIndexBuilt();
    for (const std::string &filePath : filePaths) {
        if (!filePath.ends_with(PrefabUtility::kPrefabExtension)) continue;

        std::error_code ec;
        const bool exists = std::filesystem::is_regular_file(ProjectPaths::ToPhysical(filePath), ec);
        if (ec || !exists) {
            UnregisterPrefabPath(filePath, false);
            continue;
        }

        JSON newJson = LoadJSON(ProjectPaths::ToPhysical(filePath));
        if (!newJson.is_object()) continue;
        const UUID128 newID(newJson.value("prefabID", std::string{}));
        if (!newID.IsValid()) continue;

        auto pathIt = sPathToID_.find(filePath);
        if (pathIt != sPathToID_.end() && pathIt->second != newID) {
            const UUID128 oldID = pathIt->second;
            sPathToID_.erase(pathIt);
            auto idIt = sIDToPath_.find(oldID);
            if (idIt != sIDToPath_.end() && idIt->second == filePath) sIDToPath_.erase(idIt);
            sCache_.erase(oldID);
        }

        const auto cacheIt = sCache_.find(newID);
        const JSON oldJson = cacheIt != sCache_.end() ? cacheIt->second : JSON();
        sIDToPath_[newID] = filePath;
        sPathToID_[filePath] = newID;
        if (!oldJson.is_null() && oldJson != newJson) {
            NotifyChanged(newID, oldJson, newJson);
        } else {
            sCache_[newID] = std::move(newJson);
        }
    }
}

void PrefabAssetManager::SetChangeListener(ChangeCallback callback) {
    sListener_ = std::move(callback);
}

void PrefabAssetManager::EnsureIndexBuilt() {
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
    sCache_[prefabID] = newJson;
    if (sListener_) sListener_(prefabID, oldJson, newJson);
}

} // namespace KashipanEngine
#endif // USE_IMGUI
