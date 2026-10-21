#pragma once
#ifdef USE_IMGUI

#include <cstddef>
#include <string>
#include <vector>

#include "Utilities/FileIO/JSON.h"
#include "Utilities/UUID128.h"

namespace KashipanEngine {

class SceneEditorContext;

namespace PrefabSceneSync {

/// @brief 1シーン分のPrefab同期結果
struct SceneSyncResult {
    size_t instancesMatched = 0;
    size_t objectPropertiesChanged = 0;
    size_t componentsChanged = 0;
    size_t nodesAdded = 0;
    size_t nodesRemoved = 0;
    size_t nodesReparented = 0;

    bool IsChanged() const {
        return objectPropertiesChanged != 0 || componentsChanged != 0 ||
               nodesAdded != 0 || nodesRemoved != 0 || nodesReparented != 0;
    }
};

/// @brief 登録済みシーン全体のPrefab同期結果
struct RegisteredScenesSyncResult {
    size_t scenesScanned = 0;
    size_t scenesChanged = 0;
    size_t instancesMatched = 0;
    std::vector<std::string> failedScenes;
};

/// @brief シーンJSON内にある指定Prefabの全インスタンスを三者マージで同期する
/// @details 変更前Prefabと一致する値だけを変更後Prefabへ更新するため、インスタンス側の
///          コンポーネント／オブジェクトプロパティ／親子関係Overrideは維持される。
///          Prefab側で追加されたノードは新しいobjectIDを割り当てて追加し、削除されたノードは
///          対応するインスタンス側サブツリーごと削除する。
SceneSyncResult SyncSceneJson(
    JSON &sceneJson,
    const UUID128 &prefabID,
    const JSON &oldPrefabJson,
    const JSON &newPrefabJson);

/// @brief SceneManagerに登録されている全シーンを読み込み、指定Prefabを同期して保存する
/// @details 単一JSON形式と.sceneフォルダー形式の双方をSceneFileIO経由で処理する。
///          filePathを持たないfactoryData登録シーンはメモリ上の登録データを更新する。
RegisteredScenesSyncResult SyncRegisteredScenes(
    SceneEditorContext *context,
    const UUID128 &prefabID,
    const JSON &oldPrefabJson,
    const JSON &newPrefabJson);

} // namespace PrefabSceneSync
} // namespace KashipanEngine

#endif // USE_IMGUI
