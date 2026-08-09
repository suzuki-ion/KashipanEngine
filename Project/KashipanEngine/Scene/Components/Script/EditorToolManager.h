#pragma once
#ifdef USE_IMGUI
#include <map>
#include <string>
#include <vector>

class asIScriptEngine;
class asIScriptContext;
class asIScriptFunction;
class asIScriptObject;
class asITypeInfo;

namespace KashipanEngine {

class SceneContext;

/// @brief エディターツール用スクリプト（EditorToolsフォルダの.as）を管理するクラス
/// @details 実行ディレクトリ直下（プロジェクトと同じ階層）の "EditorTools" フォルダにある.asを
///          初回フレームで自動読み込みし、EditorToolインターフェースを実装したクラスごとに
///          インスタンスを生成して以下のメソッドを自動で呼び出す:
///          - InitializeOnLoad(): 読み込み直後に一度だけ
///          - Update(): 毎フレーム（[EditorWindow]のウィンドウが開いている間は、そのウィンドウのBegin/Endの中で呼ばれる）
///          - OnItemSelected(tag): [MenuItem("MenuBar/..."または"Hierarchy/...", "tag")]で追加した項目の選択時
///          - OnWindowEnable(name) / OnWindowDisable(name): [EditorWindow("name")]のウィンドウが開閉した時
///          ウィンドウは用意されるだけで、開くのはスクリプト側（OpenEditorWindowグローバル関数）に委ねる
class EditorToolManager final {
public:
    static EditorToolManager &GetInstance();

    EditorToolManager(const EditorToolManager &) = delete;
    EditorToolManager &operator=(const EditorToolManager &) = delete;
    EditorToolManager(EditorToolManager &&) = delete;
    EditorToolManager &operator=(EditorToolManager &&) = delete;

    /// @brief 毎フレームの先頭でSceneEditorから呼ぶ。初回呼び出し時にEditorToolsフォルダを読み込む
    /// @param sceneContext 現在開いているシーンのコンテキスト（ツール内のGetScene()が返す対象）
    void BeginFrame(SceneContext *sceneContext);

    /// @brief メインメニューバーの中（BeginMainMenuBar～EndMainMenuBar）から呼ぶ。"MenuBar/"項目を描画する
    void ShowMenuBarItems();

    /// @brief ヒエラルキーの右クリックメニューの中（BeginPopup～EndPopup）から呼ぶ。"Hierarchy/"項目を描画する
    void ShowHierarchyMenuItems();

    /// @brief ウィンドウの開閉遷移（OnWindowEnable/Disable）の通知と、各ツールのUpdate呼び出しを行う
    /// @details SceneEditor::ShowImGuiから毎フレーム呼ぶ
    void UpdateTools();

    //==================================================
    // スクリプトのグローバル関数（OpenEditorWindow等）から呼ばれる操作
    //==================================================

    /// @brief [EditorWindow]で用意されたウィンドウを開く（該当する名前が無い場合は何もしない）
    void OpenWindow(const std::string &name);
    /// @brief [EditorWindow]で用意されたウィンドウを閉じる（該当する名前が無い場合は何もしない）
    void CloseWindow(const std::string &name);
    /// @brief 指定した名前のウィンドウが開いているかどうか
    bool IsWindowOpen(const std::string &name) const;

private:
    EditorToolManager() = default;
    ~EditorToolManager();

    /// @brief 1つのメニュー項目（どのツールのOnItemSelectedへどのタグを渡すか）
    struct MenuItemEntry final {
        std::string label;
        std::string tag;
        size_t toolIndex = 0;
    };

    /// @brief メニュー階層のノード（サブメニュー＋項目）
    struct MenuNode final {
        std::map<std::string, MenuNode> children;
        std::vector<MenuItemEntry> items;
    };

    /// @brief [EditorWindow]で用意されたウィンドウ1つ分の状態
    struct WindowEntry final {
        std::string name;
        size_t toolIndex = 0;
        bool isOpen = false;
        /// @brief 前フレームの開閉状態（OnWindowEnable/Disableの遷移検知用）
        bool wasOpen = false;
    };

    /// @brief EditorToolを実装したスクリプトクラス1つ分のインスタンスとメソッドキャッシュ
    struct ToolInstance final {
        asIScriptObject *object = nullptr;
        asITypeInfo *type = nullptr;
        asIScriptFunction *initializeOnLoadMethod = nullptr;
        asIScriptFunction *onItemSelectedMethod = nullptr;
        asIScriptFunction *onWindowEnableMethod = nullptr;
        asIScriptFunction *onWindowDisableMethod = nullptr;
        asIScriptFunction *updateMethod = nullptr;
        /// @brief このツールが[EditorWindow]で宣言したウィンドウのインデックス一覧
        std::vector<size_t> windowIndices;
    };

    void EnsureLoaded();
    void LoadTools();
    void RegisterMenuItem(const std::string &path, const std::string &tag, size_t toolIndex);
    void ShowMenuNode(const MenuNode &node);
    void CallToolMethod(ToolInstance &tool, asIScriptFunction *method, const std::string *stringArg);

    bool loaded_ = false;
    asIScriptEngine *engine_ = nullptr;
    asIScriptContext *context_ = nullptr;
    std::vector<ToolInstance> tools_;
    std::vector<WindowEntry> windows_;
    MenuNode menuBarRoot_;
    MenuNode hierarchyRoot_;
    SceneContext *currentSceneContext_ = nullptr;
};

} // namespace KashipanEngine
#endif // USE_IMGUI
