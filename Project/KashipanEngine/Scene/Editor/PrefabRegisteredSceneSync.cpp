#include "PrefabSceneSync.h"
#ifdef USE_IMGUI

#include <unordered_set>

#include "Scene/SceneEditorContext.h"
#include "Scene/SceneFileIO.h"
#include "Scene/SceneManager.h"

namespace KashipanEngine {
namespace PrefabSceneSync {

RegisteredScenesSyncResult SyncRegisteredScenes(
    SceneEditorContext *context,
    const UUID128 &prefabID,
    const JSON &oldPrefabJson,
    const JSON &newPrefabJson) {
    RegisteredScenesSyncResult result;
    SceneManager *sceneManager = context ? context->GetSceneManager() : nullptr;
    if (!sceneManager) return result;

    const std::vector<SceneManager::SceneEntry> entries = sceneManager->GetRegisteredScenes();
    std::unordered_set<std::string> processedPaths;
    for (const auto &entry : entries) {
        ++result.scenesScanned;

        if (entry.filePath.empty()) {
            JSON sceneJson = entry.factoryData;
            if (sceneJson.empty()) continue;
            const SceneSyncResult sceneResult =
                SyncSceneJson(sceneJson, prefabID, oldPrefabJson, newPrefabJson);
            result.instancesMatched += sceneResult.instancesMatched;
            if (sceneResult.IsChanged()) {
                sceneManager->UpdateRegisteredSceneData(entry.name, sceneJson);
                ++result.scenesChanged;
            }
            continue;
        }

        // 同じファイルが別名で複数登録されている場合は一度だけ処理する。
        if (!processedPaths.insert(entry.filePath).second) continue;
        JSON sceneJson = LoadSceneFromPath(entry.filePath);
        if (sceneJson.empty()) {
            result.failedScenes.push_back(entry.name + " (" + entry.filePath + ")");
            continue;
        }

        const SceneSyncResult sceneResult =
            SyncSceneJson(sceneJson, prefabID, oldPrefabJson, newPrefabJson);
        result.instancesMatched += sceneResult.instancesMatched;
        if (!sceneResult.IsChanged()) continue;
        if (!SaveSceneToPath(sceneJson, entry.filePath)) {
            result.failedScenes.push_back(entry.name + " (" + entry.filePath + ")");
            continue;
        }
        ++result.scenesChanged;
    }
    return result;
}

} // namespace PrefabSceneSync
} // namespace KashipanEngine

#endif // USE_IMGUI
