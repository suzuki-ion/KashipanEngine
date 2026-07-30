#pragma once
#ifdef USE_IMGUI
#include <functional>
#include <string>
#include <unordered_map>

#include "Utilities/FileIO/JSON.h"
#include "Utilities/UUID128.h"

namespace KashipanEngine {

/// @brief Prefabアセット（.prefabファイル）の資産管理クラス
/// @details prefabID（UUID128）とファイルパスの対応を索引化し、JSONのキャッシュ・保存を一元管理する。
///          「元Prefabが書き変わればシーン上のインスタンスも書き変わる」機能の起点となるハブ：
///          SavePrefabJson系のAPIを経由して保存が行われるたびに、登録済みリスナーへ変更を通知する
///          （エンジン内操作時にのみ反映され、常時ファイル監視は行わない）。
class PrefabAssetManager final {
public:
    /// @brief Prefabファイルが（エンジン内操作により）変更された際に呼ばれるコールバック
    using ChangeCallback = std::function<void(const UUID128 &prefabID, const JSON &oldJson, const JSON &newJson)>;

    /// @brief prefabIDからファイルパス（実行ディレクトリ相対）を取得する（未登録の場合は空文字）
    static std::string GetPrefabPath(const UUID128 &prefabID);
    /// @brief ファイルパスからprefabIDを取得する（未登録・無効なJSONの場合は無効なUUID128）
    static UUID128 GetPrefabIDFromPath(const std::string &filePath);

    /// @brief 新規Prefabファイルを作成する（実ファイルへの書き込み＋索引・キャッシュへの登録）
    static bool CreatePrefabFile(const UUID128 &prefabID, const JSON &json, const std::string &filePath);

    /// @brief prefabIDからJSONを取得する（キャッシュ優先。未キャッシュなら索引から解決してロードする）
    static JSON LoadPrefabJson(const UUID128 &prefabID);

    /// @brief 登録済みprefabIDのJSONを書き込み、キャッシュ更新後にリスナーへ通知する
    /// @details Apply All・個別コンポーネントApplyから呼ばれるメインの書き込みAPI
    static bool SavePrefabJson(const UUID128 &prefabID, const JSON &newJson);
    /// @brief ファイルパス起点での保存（AssetsウィンドウのJSONエディター用）
    /// @details newJson["prefabID"]からprefabIDを解決してSavePrefabJsonへ委譲する。
    ///          prefabIDキーが無い/無効な通常の.jsonファイルの場合は素通しでSaveJSONするだけ（伝播なし）
    static bool SavePrefabJsonByPath(const std::string &filePath, const JSON &newJson);

    /// @brief Assetsウィンドウのリネーム機構から呼ばれる、索引の追従（実ファイルは操作しない）
    static bool RenamePrefabFile(const std::string &oldFilePath, const std::string &newFilePath);

    /// @brief 変更通知リスナーを設定する（既存の登録があれば置き換える）
    /// @details シーン切り替えのたびにSceneEditorが再構築されるため、リスナーは「常に現在の
    ///          SceneEditorのものだけ」が有効であるべき。蓄積するリスト（複数登録）にすると、
    ///          破棄済みのSceneEditor/SceneEditorContextを指した古いコールバックが呼ばれてしまう
    ///          （ダングリングポインタ）ため、単一スロットの置き換え方式にしている
    static void SetChangeListener(ChangeCallback callback);

private:
    /// @brief Assetsフォルダを走査し、既存.prefabファイルのprefabIDを索引化する
    /// @details セッション中、未構築の場合にのみ実行される（常時ファイル監視はしない方針のため、
    ///          以後はエンジン内操作（CreatePrefabFile/RenamePrefabFile）でのみ増分更新される）
    static void EnsureIndexBuilt();
    /// @brief キャッシュへjsonを登録し、リスナーへ通知する（保存前後のJSONを渡す）
    static void NotifyChanged(const UUID128 &prefabID, const JSON &oldJson, const JSON &newJson);

    static inline std::unordered_map<UUID128, std::string> sIDToPath_;
    static inline std::unordered_map<std::string, UUID128> sPathToID_;
    static inline std::unordered_map<UUID128, JSON> sCache_;
    static inline ChangeCallback sListener_;
    static inline bool sIndexBuilt_ = false;
};

} // namespace KashipanEngine
#endif // USE_IMGUI
