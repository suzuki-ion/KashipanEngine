#pragma once
#ifdef USE_IMGUI
#include <memory>
#include <string>

#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {

class SceneEditorCommands;
class SceneObjectHierarchy;
class SceneObjectInspector;
class SceneComponentInspector;
class SceneVariablesMenu;
class SceneEditorView;
class AssetsWindow;
class SceneSaver;
class SceneLoader;
class SceneListEditor;
class EditorPreferences;

class SceneEditor final {
public:
    SceneEditor(Passkey<Scene>, SceneEditorContext *context);
    ~SceneEditor();

    void ShowImGui();

private:
    void ShowMainWindow();
    void ShowPlayControls();
    /// @brief シーン新規作成の確認モーダル（既存内容を破棄する前に確認する）
    /// @return このフレームでシーンが新規作成された場合は true
    bool ShowNewSceneModal();
    void HandleShortcuts();
    void PerformUndo();
    void PerformRedo();
    /// @brief 設定した間隔でシーンを kSceneBackupDirectory（"SceneBackups/"）以下へ自動保存する
    void HandleAutoSave();
    /// @brief 自動保存の間隔・保存名（TemplateLiteral）を設定するモーダル
    void ShowAutoSaveSettingsModal();
    /// @brief シーンのバックアップを1回取る
    /// @param prefix 保存ファイル名の先頭に付与する文字列（呼び出し元の種別を見分けるため）
    void TakeSceneBackup(const std::string &prefix);

    SceneEditorContext *context_ = nullptr;

    std::unique_ptr<SceneEditorCommands> commands_;
    std::unique_ptr<SceneObjectHierarchy> objectHierarchy_;
    std::unique_ptr<SceneObjectInspector> objectInspector_;
    std::unique_ptr<SceneComponentInspector> componentInspector_;
    std::unique_ptr<SceneVariablesMenu> variablesMenu_;
    std::unique_ptr<SceneEditorView> sceneView_;
    std::unique_ptr<AssetsWindow> assetsWindow_;
    std::unique_ptr<SceneSaver> saver_;
    std::unique_ptr<SceneLoader> loader_;
    std::unique_ptr<SceneListEditor> sceneListEditor_;
    std::unique_ptr<EditorPreferences> preferences_;

    bool isShowSceneView_ = true;
    bool isShowHierarchy_ = true;
    bool isShowObjectInspector_ = true;
    bool isShowComponentInspector_ = true;
    bool isShowVariablesMenu_ = true;
    bool isShowAssets_ = true;
    bool isShowSceneList_ = true;
    bool isShowHistory_ = true;
    bool isShowPreferences_ = false;

    // デバッグ用ウィンドウ表示フラグ（旧ImGuiManagerから移設）
    bool isShowLoadedTexturesWindow_ = false;
    bool isShowLoadedModelsWindow_ = false;
    bool isShowMaterialsWindow_ = false;
    bool isShowLoadedSoundsWindow_ = false;
    bool isShowPlayingSoundsWindow_ = false;
    bool isShowLoggerWindow_ = true;
    bool isShowImGuiDemoWindow_ = false;
    bool isShowInputStateWindow_ = false;
    bool isShowInputCommandEditorWindow_ = false;

    // シーン新規作成の確認モーダル用
    bool isNewSceneRequested_ = false;
    std::string newSceneName_;

    /// @brief 前フレームの再生状態（コマンド履歴の再生セッション切り替えの検知用）
    bool wasPlaying_ = false;

    /// @brief 自動保存の前回保存からの経過時間（秒）
    float autoSaveElapsedTime_ = 0.0f;
    /// @brief 自動保存の間隔（分）。エディター上で変更可能で、再起動後も維持される
    float autoSaveIntervalMinutes_ = 1.0f;
    /// @brief 自動保存のファイル名テンプレート（TemplateLiteralの `${name}` 記法。拡張子は自動付与）
    std::string autoSaveNameFormat_ = "${SceneName}";
    /// @brief 自動保存設定モーダルの表示要求
    bool isAutoSaveSettingsRequested_ = false;
};

} // namespace KashipanEngine
#endif // USE_IMGUI
