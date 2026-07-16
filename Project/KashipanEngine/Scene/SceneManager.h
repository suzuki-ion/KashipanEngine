#pragma once
#include "Scene/Scene.h"
#include "Utilities/MyAny.h"

#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace KashipanEngine {

class GameEngine;

class SceneManager {
public:
    /// @brief シーンリスト定義ファイルのデフォルト保存先
    static constexpr const char *kDefaultSceneListFilePath = "Assets/KashipanEngine/SceneList.json";

    /// @brief 登録済みシーン1件分の情報
    struct SceneEntry {
        /// @brief シーン名（登録・切り替えのキー）
        std::string name;
        /// @brief シーン切り替え時に読み込むシーンJSONファイルのパス（空の場合は factoryData / 空シーン）
        std::string filePath;
        /// @brief 直接JSONを渡して登録された場合のファクトリデータ（filePath が空の場合のみ使用）
        JSON factoryData;
    };

    SceneManager(Passkey<GameEngine>) { sActiveInstance_ = this; }
    ~SceneManager() {
        if (sActiveInstance_ == this) sActiveInstance_ = nullptr;
    }

    SceneManager(const SceneManager &) = delete;
    SceneManager &operator=(const SceneManager &) = delete;
    SceneManager(SceneManager &&) = delete;
    SceneManager &operator=(SceneManager &&) = delete;

    /// @brief 現在のシーンを取得する
    /// @return 現在のシーンのポインタ（存在しない場合は nullptr）
    const Scene *GetCurrentScene() const {
        if (currentScene_) return currentScene_.get();
        return nullptr;
    }

    /// @brief クラッシュハンドラから現在アクティブなSceneManagerを取得する
    /// @details エンジン実行中は常に唯一のSceneManagerが動いている前提で、
    ///          最後に生存しているインスタンスを保持しているだけなので、
    ///          通常のゲームロジックからは使用しないこと。
    static const SceneManager *GetActiveInstance(PasskeyForCrashHandler) { return sActiveInstance_; }

    //==================================================
    // シーンの登録・登録情報の編集
    //==================================================

    /// @brief シーンの登録（JSONデータを直接渡す方式）
    /// @param sceneName 登録するシーンの名前
    /// @param factoryData シーンのファクトリデータ（JSON形式）
    /// @return 登録に成功した場合は true、失敗した場合は false を返す
    bool RegisterScene(const std::string &sceneName, const JSON &factoryData = JSON());
    /// @brief シーンの登録（切り替え時にJSONファイルを読み込む方式）
    /// @param sceneName 登録するシーンの名前
    /// @param filePath シーン切り替え時に読み込むシーンJSONファイルのパス
    /// @return 登録に成功した場合は true、失敗した場合は false を返す
    bool RegisterSceneFile(const std::string &sceneName, const std::string &filePath);
    /// @brief シーンの登録を解除する
    /// @param sceneName 登録解除するシーンの名前
    /// @return 解除に成功した場合は true、失敗した場合は false を返す
    bool UnregisterScene(const std::string &sceneName);
    /// @brief 登録されているシーンの名前を変更する
    /// @param oldName 変更前のシーン名
    /// @param newName 変更後のシーン名（既に同名のシーンが登録されている場合は失敗する）
    /// @return 変更に成功した場合は true、失敗した場合は false を返す
    bool RenameRegisteredScene(const std::string &oldName, const std::string &newName);
    /// @brief 登録されているシーンの読み込みファイルパスを変更する
    /// @param sceneName 対象のシーン名
    /// @param filePath 新しいシーンJSONファイルのパス
    /// @return 変更に成功した場合は true、失敗した場合は false を返す
    bool SetRegisteredSceneFilePath(const std::string &sceneName, const std::string &filePath);
    /// @brief 登録されているシーンのファクトリデータを更新する
    /// @param sceneName 更新するシーンの名前
    /// @param factoryData 更新するシーンのファクトリデータ（JSON形式）
    /// @return 更新に成功した場合は true、失敗した場合は false を返す
    bool UpdateRegisteredSceneData(const std::string &sceneName, const JSON &factoryData);

    /// @brief 登録されているシーンの一覧を取得する（登録順）
    const std::vector<SceneEntry> &GetRegisteredScenes() const { return registeredScenes_; }
    /// @brief シーンが登録されているかを確認する
    bool IsSceneRegistered(const std::string &sceneName) const { return FindEntry(sceneName) != nullptr; }

    //==================================================
    // シーンリスト定義ファイル（JSON）の読み書き
    //==================================================

    /// @brief シーンリスト定義ファイルを読み込み、記載されたシーンを登録する
    /// @details 既に同名のシーンが登録されている場合、そのシーンはスキップされる。
    ///          "startupScene" が記載されている場合はスタートアップシーン名として保持する
    /// @param filePath シーンリスト定義ファイルのパス
    /// @return 読み込みに成功した場合は true（ファイルが存在しない・不正な場合は false）
    bool LoadSceneList(const std::string &filePath = kDefaultSceneListFilePath);
    /// @brief 現在の登録内容をシーンリスト定義ファイルへ保存する
    /// @details ファイルパスを持たない（JSON直接登録の）シーンは filePath 空文字で保存される
    /// @param filePath シーンリスト定義ファイルのパス
    /// @return 保存に成功した場合は true
    bool SaveSceneList(const std::string &filePath = kDefaultSceneListFilePath) const;

    /// @brief 起動時に自動的に切り替わるシーン名を取得する
    const std::string &GetStartupSceneName() const { return startupSceneName_; }
    /// @brief 起動時に自動的に切り替わるシーン名を設定する
    void SetStartupSceneName(const std::string &sceneName) { startupSceneName_ = sceneName; }
    /// @brief スタートアップシーンへの切り替えを予約する
    /// @return 切り替えに成功した場合は true（スタートアップシーンが未設定・未登録の場合は false）
    bool ChangeToStartupScene() { return startupSceneName_.empty() ? false : ChangeScene(startupSceneName_); }

    /// @brief シーンの更新処理
    void Update(Passkey<GameEngine>);
#ifdef USE_IMGUI
    /// @brief ImGuiの描画処理
    void ShowImGui(Passkey<GameEngine>);
#endif

    //==================================================
    // シーン切り替え系
    //==================================================

    /// @brief シーンを変更する（次のフレームで変更が反映される）
    /// @param sceneName 変更したいシーンの名前
    /// @return 変更に成功した場合は true、失敗した場合は false を返す
    bool ChangeScene(const std::string &sceneName);
    /// @brief シーン変更の予約があるかを確認する
    bool HasPendingSceneChange() const { return hasPendingSceneChange_; }
    /// @brief 保留中のシーン変更をコミットする（GameEngine専用）
    /// @return コミットに成功した場合は true、失敗した場合は false を返す
    bool CommitPendingSceneChange(Passkey<GameEngine>);

    //==================================================
    // シーン変数
    //==================================================

    /// @brief シーン変数を追加する
    /// @tparam T 変数の型
    /// @param key 変数のキー
    /// @param value 変数の値
    /// @return 追加されたシーン変数のポインタ
    template <typename T>
    MyAny *AddGlobalSceneVariable(const std::string &key, const T &value = T()) { return AddGlobalSceneVariable(key, value, GetValueType<T>()); }
    /// @brief シーン変数を追加する
    /// @param key 変数のキー
    /// @param value 変数の値（MyAny 型）
    /// @param typeInfo 変数の型情報
    /// @return 追加されたシーン変数のポインタ
    MyAny *AddGlobalSceneVariable(const std::string &key, const MyAny &value, const TypeInfo &typeInfo);
    /// @brief シーン変数を削除する
    /// @param key 変数のキー
    /// @return 削除に成功した場合は true、失敗した場合は false を返す
    bool RemoveGlobalSceneVariable(const std::string &key) { return globalSceneVariables_.erase(key) > 0; }
    /// @brief シーン変数の情報を取得する
    /// @param key 変数のキー
    /// @return シーン変数のポインタ（存在しない場合は nullptr）
    MyAny *GetGlobalSceneVariable(const std::string &key);
    /// @brief シーン変数の型を取得する
    /// @param key 変数のキー
    /// @return シーン変数の型情報
    const TypeInfo &GetGlobalSceneVariableTypeInfo(const std::string &key);
    /// @brief シーン変数を取得する
    /// @return シーン変数のマップ
    const std::unordered_map<std::string, MyAny> &GetGlobalSceneVariables() const { return globalSceneVariables_; }

private:
    /// @brief 名前から登録エントリを検索する（存在しない場合は nullptr）
    SceneEntry *FindEntry(const std::string &sceneName);
    const SceneEntry *FindEntry(const std::string &sceneName) const;

    static inline SceneManager *sActiveInstance_ = nullptr;

    std::vector<SceneEntry> registeredScenes_;
    std::string startupSceneName_;
    std::unordered_map<std::string, MyAny> globalSceneVariables_;

    std::unique_ptr<Scene> currentScene_;
    bool hasPendingSceneChange_ = false;
    std::string pendingSceneName_;
};

} // namespace KashipanEngine
