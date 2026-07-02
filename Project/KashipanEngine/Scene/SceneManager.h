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

namespace KashipanEngine {

class GameEngine;

class SceneManager {
public:
    SceneManager(Passkey<GameEngine>) {}
    ~SceneManager() = default;

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

    /// @brief シーンの登録
    /// @param sceneName 登録するシーンの名前
    /// @param factoyData シーンのファクトリデータ（JSON形式）
    /// @return 登録に成功した場合は true、失敗した場合は false を返す
    bool RegisterScene(const std::string &sceneName, const JSON &factoryData = JSON());
    /// @brief 登録されているシーンのファクトリデータを更新する
    /// @param sceneName 更新するシーンの名前
    /// @param factoryData 更新するシーンのファクトリデータ（JSON形式）
    /// @return 更新に成功した場合は true、失敗した場合は false を返す
    bool UpdateRegisteredSceneData(const std::string &sceneName, const JSON &factoryData);

    /// @brief 登録されているシーンの一覧を取得する
    /// @return 登録されているシーンの名前のリスト
    const std::unordered_map<std::string, JSON> &GetRegisteredScenes() const { return sceneFactoriesData_; }

    /// @brief シーンの更新処理
    void Update(Passkey<GameEngine>);

    //==================================================
    // シーン切り替え系
    //==================================================

    /// @brief シーンを変更する（次のフレームで変更が反映される）
    /// @param sceneName 変更したいシーンの名前
    /// @return 変更に成功した場合は true、失敗した場合は false を返す
    bool ChangeScene(const std::string &sceneName);
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
    std::unordered_map<std::string, JSON> sceneFactoriesData_;
    std::unordered_map<std::string, MyAny> globalSceneVariables_;

    std::unique_ptr<Scene> currentScene_;
    bool hasPendingSceneChange_ = false;
    std::string pendingSceneName_;
};

} // namespace KashipanEngine
