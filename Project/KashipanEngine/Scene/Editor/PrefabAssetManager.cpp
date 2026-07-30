#include "PrefabAssetManager.h"
#ifdef USE_IMGUI
#include "Scene/Editor/PrefabUtility.h"
#include "Utilities/FileIO/Directory.h"

namespace KashipanEngine {

namespace {
void CollectFilesRecursive(const DirectoryData &dir, std::vector<std::string> &out) {
    for (const auto &file : dir.files) out.push_back(file);
    for (const auto &subdir : dir.subdirectories) CollectFilesRecursive(subdir, out);
}
} // namespace

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
    if (!prefabID.IsValid() || !SaveJSON(json, filePath)) return false;
    sIDToPath_[prefabID] = filePath;
    sPathToID_[filePath] = prefabID;
    sCache_[prefabID] = json;
    return true;
}

JSON PrefabAssetManager::LoadPrefabJson(const UUID128 &prefabID) {
    EnsureIndexBuilt();
    auto cacheIt = sCache_.find(prefabID);
    if (cacheIt != sCache_.end()) return cacheIt->second;

    auto pathIt = sIDToPath_.find(prefabID);
    if (pathIt == sIDToPath_.end()) return JSON();
    JSON json = LoadJSON(pathIt->second);
    sCache_[prefabID] = json;
    return json;
}

bool PrefabAssetManager::SavePrefabJson(const UUID128 &prefabID, const JSON &newJson) {
    EnsureIndexBuilt();
    auto pathIt = sIDToPath_.find(prefabID);
    if (pathIt == sIDToPath_.end()) return false;
    if (!SaveJSON(newJson, pathIt->second)) return false;

    const JSON oldJson = LoadPrefabJson(prefabID);
    NotifyChanged(prefabID, oldJson, newJson);
    return true;
}

bool PrefabAssetManager::SavePrefabJsonByPath(const std::string &filePath, const JSON &newJson) {
    EnsureIndexBuilt();
    const UUID128 prefabID(newJson.value("prefabID", std::string{}));
    if (!prefabID.IsValid()) {
        // prefabIDを持たない通常のJSONファイル（.prefabでも未登録・壊れている場合）はそのまま保存するだけ
        return SaveJSON(newJson, filePath);
    }
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

void PrefabAssetManager::SetChangeListener(ChangeCallback callback) {
    sListener_ = std::move(callback);
}

void PrefabAssetManager::EnsureIndexBuilt() {
    if (sIndexBuilt_) return;
    sIndexBuilt_ = true;

    const DirectoryData dir = GetDirectoryDataByExtension("Assets", { PrefabUtility::kPrefabExtension }, true, true);
    std::vector<std::string> files;
    CollectFilesRecursive(dir, files);
    for (const auto &filePath : files) {
        JSON json = LoadJSON(filePath);
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
