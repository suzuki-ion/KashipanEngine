#pragma once
#ifdef USE_IMGUI
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Utilities/FileIO/JSON.h"
#include "Utilities/UUID128.h"

namespace KashipanEngine {

/// @brief Prefabアセット（.prefabファイル）の資産管理クラス
/// @details prefabID（UUID128）とファイルパスの対応を索引化し、JSONのキャッシュ・保存を一元管理する。
///          「元Prefabが書き変わればシーン上のインスタンスも書き変わる」機能の起点となるハブ：
///          SavePrefabJson系のAPIを経由して保存が行われるたびに、登録済みリスナーへ変更を通知する
///          SceneEditorのファイル監視から外部変更を受け取った場合も同じ変更通知経路を使用する。
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
    /// @brief LoadPrefabJsonのコピーを避ける参照版。キャッシュ済みJSONへの参照をそのまま返す
    /// @details nlohmann::jsonは値型のためLoadPrefabJsonは呼ぶたびにPrefab全体をディープコピーする。
    ///          ヒエラルキーパネルの毎行チェックのように高頻度・読み取り専用で呼ばれる箇所向け。
    ///          返す参照はSavePrefabJson等でキャッシュが更新されるまでの間のみ有効（同一フレーム内の
    ///          読み取り専用処理での使用を想定し、結果を長期間保持しないこと）
    static const JSON &LoadPrefabJsonRef(const UUID128 &prefabID);

    /// @brief 登録済みprefabIDのJSONを書き込み、キャッシュ更新後にリスナーへ通知する
    /// @details Apply All・個別コンポーネントApplyから呼ばれるメインの書き込みAPI
    static bool SavePrefabJson(const UUID128 &prefabID, const JSON &newJson);
    /// @brief ファイルパス起点での保存（AssetsウィンドウのJSONエディター用）
    /// @details newJson["prefabID"]からprefabIDを解決してSavePrefabJsonへ委譲する。
    ///          prefabIDキーが無い/無効な通常の.jsonファイルの場合は素通しでSaveJSONするだけ（伝播なし）
    static bool SavePrefabJsonByPath(const std::string &filePath, const JSON &newJson);

    /// @brief Assetsウィンドウのリネーム機構から呼ばれる、索引の追従（実ファイルは操作しない）
    static bool RenamePrefabFile(const std::string &oldFilePath, const std::string &newFilePath);
    /// @brief フォルダ改名時に、配下の全Prefabパスを新しいフォルダへ追従させる
    static void RenamePrefabFolder(const std::string &oldFolderPath, const std::string &newFolderPath);
    /// @brief 削除されたPrefabファイル、またはフォルダ配下のPrefabを索引とキャッシュから解除する
    static void UnregisterPrefabPath(const std::string &fileOrFolderPath, bool recursive);

    /// @brief ファイル監視で検知したPrefabをディスクから再読込し、変更通知を発行する
    static void ReloadExternallyChangedFiles(const std::vector<std::string> &filePaths);

    /// @brief 変更通知リスナーを設定する（既存の登録があれば置き換える）
    /// @details シーン切り替えのたびにSceneEditorが再構築されるため、リスナーは「常に現在の
    ///          SceneEditorのものだけ」が有効であるべき。蓄積するリスト（複数登録）にすると、
    ///          破棄済みのSceneEditor/SceneEditorContextを指した古いコールバックが呼ばれてしまう
    ///          （ダングリングポインタ）ため、単一スロットの置き換え方式にしている
    static void SetChangeListener(ChangeCallback callback);

private:
    /// @brief Assetsフォルダを走査し、既存.prefabファイルのprefabIDを索引化する
    /// @details セッション中、未構築の場合にのみ実行される。以後はエンジン内操作と
    ///          SceneEditorのファイル監視通知により増分更新される
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
