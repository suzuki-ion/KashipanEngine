#pragma once
#ifdef USE_IMGUI
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "Utilities/Passkeys.h"
#include "Scene/Editor/AssetEditorWindows.h"

namespace KashipanEngine {

class EmptyObject;
class SceneEditor;
class SceneEditorContext;
class SceneEditorCommands;

/// @brief Unity の Assets ウィンドウ風のファイルブラウザ
/// @details 実行中のディレクトリ以下から、エンジンが対応している形式のファイルを表示する。
///          左側にフォルダツリー、右側に選択中フォルダ内のファイルをグリッド表示する。
class AssetsWindow final {
public:
    AssetsWindow(Passkey<SceneEditor>, SceneEditorContext *editorContext);
    ~AssetsWindow();

    /// @brief Undo/Redo用のコマンド管理を設定する（Prefab付与などをUndo対応させるため）
    void SetCommands(SceneEditorCommands *commands) { commands_ = commands; }

    void ShowImGui();

    /// @brief OSからウィンドウ全体へD&Dされたファイルを、現在アクティブなAssetsWindowが
    ///        開いているフォルダへ取り込む（コピー＋対応するAssetManagerでの動的読み込み）
    /// @details AssetsWindowが1つも生成されていない場合は何もしない
    /// @param physicalPaths ドロップされたファイルの物理パス（絶対パス）一覧
    static void HandleDroppedFiles(const std::vector<std::string> &physicalPaths);

private:
    struct FolderNode {
        std::string name;
        std::string path; // 実行ディレクトリからの相対パス（ルートは空文字）
        std::vector<std::unique_ptr<FolderNode>> children;
    };

    struct FileEntry {
        std::string name;
        std::string path;
        std::string extension; // 小文字
        bool isFolder = false;
    };

    /// @brief エンジンが対応している形式か（拡張子は小文字で渡す）
    static bool IsSupportedExtension(const std::string &ext);
    /// @brief 拡張子の分類色（Unityのアイコンの代わり）
    static unsigned int ExtensionColor(const std::string &ext);
    static std::string ToLowerExtension(const std::filesystem::path &path);

    /// @brief フォルダツリーを再構築する
    void RefreshFolderTree();
    void BuildFolderNode(FolderNode &node);
    /// @brief 選択中フォルダのファイル一覧を再取得する
    void RefreshFileList();
    /// @brief 指定フォルダへ移動する（ファイル一覧の更新に加え、左のツリーの自動展開・
    ///        スクロール同期も行う）
    void NavigateToFolder(const std::string &path);
    /// @brief 実行ディレクトリ相対パスの親フォルダのパスを返す（ルート直下なら空文字）
    static std::string GetParentFolder(const std::string &path);
    void ShowFolderNode(FolderNode &node);
    void ShowFileGrid();
    void ShowFileContextMenu(const FileEntry &file);
    void ShowGridBackgroundContextMenu();
    /// @brief ファイルを拡張子に応じた編集/プレビューウィンドウで開く（同じファイルは多重に開かない）
    void OpenFileEditor(const FileEntry &file);
    /// @brief 開いている編集/プレビューウィンドウを表示し、閉じられたものを片付ける
    void ShowOpenEditors();
    /// @brief 指定の実行ディレクトリ相対パスを開いている編集/プレビューウィンドウを閉じる
    /// @details リネームで古いパスが無効になった場合に呼ぶ
    void CloseEditorsForPath(const std::string &cwdRelativePath);
    /// @brief 新規ファイル作成モーダル（.json / .mat）を表示する
    bool ShowCreateFileModal();
    /// @brief 新規フォルダ作成モーダルを表示する
    void ShowCreateFolderModal();
    /// @brief リネームモーダルを表示する（ファイル・フォルダ共通）
    void ShowRenameModal();
    /// @brief 削除確認モーダルを表示する（ファイル・フォルダ共通）
    void ShowDeleteConfirmModal();
    /// @brief ヒエラルキーからD&Dされたオブジェクトを、現在開いているフォルダへ.prefabファイルとして保存する
    void CreatePrefabFromObject(EmptyObject *obj);
    /// @brief OSからD&Dされたファイル群を現在開いているフォルダへコピーし、拡張子に応じて
    ///        対応するAssetManagerで動的に読み込む
    void ImportDroppedFiles(const std::vector<std::string> &physicalPaths);

    // OSからのファイルD&Dの取り込み先を決めるため、現在アクティブな（最後に生成された）インスタンスを保持する
    static inline AssetsWindow *sActiveInstance_ = nullptr;

    SceneEditorContext *editorContext_ = nullptr;
    SceneEditorCommands *commands_ = nullptr;

    FolderNode rootFolder_;
    std::string currentFolder_;
    std::vector<FileEntry> files_;

    // フォルダツリーとの同期用（現在開いているフォルダまでの経路を1フレームだけ強制展開しスクロールする）
    std::unordered_set<std::string> forceOpenFolders_;
    std::string pendingScrollToFolder_;

    std::vector<std::unique_ptr<JSONFileEditorWindow>> jsonEditors_;
    std::vector<std::unique_ptr<MaterialFileEditorWindow>> materialEditors_;
    std::vector<std::unique_ptr<ImagePreviewWindow>> imagePreviews_;
    std::vector<std::unique_ptr<AudioPreviewWindow>> audioPreviews_;
    std::vector<std::unique_ptr<VideoPreviewWindow>> videoPreviews_;
    std::vector<std::unique_ptr<ModelPreviewWindow>> modelPreviews_;

    // 新規ファイル作成（空白部の右クリックメニュー）
    bool isCreateFileRequested_ = false;
    std::string createFileExtension_; // ".json" or ".mat"
    std::string newFileName_;

    // 新規フォルダ作成（空白部の右クリックメニュー）
    bool isCreateFolderRequested_ = false;
    std::string newFolderName_;

    // ファイル/フォルダアイコンの右クリックメニュー（リネーム/削除）
    std::string contextMenuTargetPath_;
    bool contextMenuTargetIsFolder_ = false;
    bool isRenameRequested_ = false;
    std::string renameBuffer_;
    bool isDeleteRequested_ = false;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
